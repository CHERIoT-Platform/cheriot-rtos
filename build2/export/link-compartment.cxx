// c++ 1 ---

// Note: this recipe is reused to link libraries besides compartments.

// For vocabulary types (string, path, vector), see <libbuild2/types.hxx>.
// For standard utilities (move(), to_string()) see <libbuild2/utility.hxx>.
//
// Include extra headers, define global functions, etc., before the `---`
// separator.

---

bool
match (action a, target& t) const
{
  tracer trace ("link_compartment::match");

  const scope& bs (t.base_scope ());

  // We only match if there is at least one prerequisite of the c{}, cxx{},
  // or obje{} target type.
  //
  const target_type& c_tt   (*bs.find_target_type ("c"));
  const target_type& cxx_tt (*bs.find_target_type ("cxx"));
  const target_type& obj_tt (*bs.find_target_type ("obje"));

  for (prerequisite_member p: group_prerequisite_members (a, t))
  {
    if (include (a, t, p) != include_type::normal) // Excluded/ad hoc.
      continue;

    if (p.is_a (c_tt) || p.is_a (cxx_tt) || p.is_a (obj_tt))
      return true;
  }

  l4 ([&]{trace << "no C/C++ source file or object file for target " << t;});
  return false;
}

recipe
apply (action a, target& xt, match_extra& me) const override
{
  context& ctx (xt.ctx);

  const scope& bs (xt.base_scope ());
  const scope& rs (*bs.root_scope ());

  file& t (xt.as<file> ()); // compartment{}, library{}, etc.

  // Derive the target path.
  //
  t.derive_path ();

  // Match prerequisites.
  //
  const fsdir* dir (inject_fsdir (a, t));

  // This is essentially match_prerequisite_members() with some dependency
  // synthesis and the pattern->apply_prerequisites() call worked in between.
  //
  const target_type& c_tt   (*bs.find_target_type ("c"));
  const target_type& cxx_tt (*bs.find_target_type ("cxx"));
  const target_type& obj_tt (*bs.find_target_type ("obje"));

  bool compart (t.is_a (*bs.find_target_type ("compartment")) ||
                t.is_a (*bs.find_target_type ("privileged_compartment")));

  // Add target's prerequisites.
  //
  auto& pts (t.prerequisite_targets[a]);
  for (prerequisite_member p: group_prerequisite_members (a, t))
  {
    // Ignore excluded.
    //
    include_type pi (include (a, t, p));
    if (!pi)
      continue;

    // If this is a c/cxx{} prerequisite, synthesize the obje{}:c/cxx{}
    // dependency. For compartments also propagate the compartment name in
    // -cheri-compartment=.
    //
    if (pi == include_type::normal && (p.is_a (c_tt) || p.is_a (cxx_tt)))
    {
      // Note: this code is inspired by libbuild2/cc/link-rule.cxx.
      //
      const prerequisite_key& cp (p.key ()); // Source key.

      // Come up with the obje{} target directory. The source prerequisite
      // directory can be relative (to the scope) or absolute. If it is
      // relative, then use it as is. If absolute, then translate it to the
      // corresponding directory under out_root. While the source directory is
      // most likely under src_root, it is also possible it is under out_root
      // (e.g., generated source).
      //
      dir_path d;
      {
        const dir_path& cpd (*cp.tk.dir);

        if (cpd.relative () || cpd.sub (rs.out_path ()))
          d = cpd;
        else
        {
          if (!cpd.sub (rs.src_path ()))
            fail << "out of project prerequisite " << cp <<
              info << "specify corresponding obje{} target explicitly";

          d = rs.out_path () / cpd.leaf (rs.src_path ());
        }
      }

      pair<target&, ulock> obj_tl (
        search_new_locked (ctx,
                           obj_tt,
                           d,
                           dir_path (), // out (always in out)
                           *cp.tk.name,
                           nullptr,
                           cp.scope));

      // Assume this is already done if the target exists (e.g., operation
      // batch).
      //
      if (obj_tl.second.owns_lock ())
      {
        // Note that here we don't need fsdir{} since we are building in the
        // same directory as the source file.
        //
        obj_tl.first.prerequisites (prerequisites {p.as_prerequisite ()});

        // Use target name as compartment name.
        //
        if (compart)
          cast<strings> (obj_tl.first.append_locked ("cc.coptions")).push_back (
            "-cheri-compartment=" + t.name);

        obj_tl.second.unlock ();
      }

      pts.push_back (prerequisite_target (obj_tl.first, pi));
      continue;
    }

    const target& pt (p.search (t));

    if (dir == &pt) // Skip if already added.
      continue;

    // Re-create the clean semantics as in match_prerequisite_members().
    //
    if (a.operation () == clean_id && !pt.in (rs))
      continue;

    pts.push_back (prerequisite_target (pt, pi));
  }

  // Inject pattern's prerequisites.
  //
  pattern->apply_prerequisites (a, t, bs, me);

  // Start asynchronous matching of prerequisites. Wait with unlocked phase to
  // allow phase switching.
  //
  wait_guard wg (ctx, ctx.count_busy (), t[a].task_count, true);

  for (const prerequisite_target& pt: pts)
    if (pt.target != dir) // Skip if already matched.
      match_async (a, *pt.target, ctx.count_busy (), t[a].task_count);

  wg.wait ();

  // Finish matching.
  //
  for (const prerequisite_target& pt: pts)
    if (pt.target != dir) // Skip if already matched.
      match_complete (a, *pt.target);

  switch (a)
  {
  case perform_update_id:  return perform_update;
  case perform_clean_id:   return perform_clean_depdb;
  default: assert (false); return noop_recipe;
  }
}

static target_state
perform_update (action a, const target& xt)
{
  tracer trace ("link_compartment::perform_update");

  context& ctx (xt.ctx);

  const scope& bs (xt.base_scope ());
  const scope& rs (*bs.root_scope ());

  const file& t (xt.as<file> ()); // compartment{}, library{}, etc.
  const path& tp (t.path ());

  process_path ld (run_search (cast<path> (rs["ld"])));

  timestamp mt (t.load_mtime ());
  optional<target_state> ps (execute_prerequisites (a, t, mt));

  auto& pts (t.prerequisite_targets[a]);

  depdb dd (tp + ".d");

  // Start building the command line. While we don't yet know whether we will
  // really need it, the easiest is to hash it and find out.
  //
  cstrings args {
    ld.recall_string (),
    "--relax",
    "--compartment", // Yes, even for libraries.
    "--script", find_if (pts.begin (), pts.end (),
                         [t = string ("ldscript")] (const prerequisite_target& pt)
                         {
                           return pt.target != nullptr &&
                             pt.target->type ().name == t;
                         })->target->as<file> ().path ().string ().c_str (),
    "-o", tp.string ().c_str ()
  };

  // Append all the obje{} input paths.
  //
  for (const prerequisite_target& pt: pts)
  {
    if (pt.target == nullptr) // Skip "holes".
      continue;

    const target& p (*pt.target);
    string t (p.type ().name);

    if (t == "obje")
      args.push_back (p.as<file> ().path ().string ().c_str ());
  }

  // Hash the command line and compare with depdb.
  //
  {
    sha256 cs;
    for (const char* a: args)
      cs.append (a);

    if (dd.expect (cs.string ()) != nullptr)
      l4 ([&]{trace << "command line mismatch forcing update of " << t;});
  }

  if (dd.writing () || dd.mtime > mt)
    ps = nullopt; // Update.

  dd.close ();

  if (ps)
    return *ps; // Everything is up-to-date.

  args.push_back (nullptr);

  if (verb == 1)
    print_diag ("ld", t);
  else if (verb >= 2)
    print_process (args);

  if (!ctx.dry_run)
  {
    run (ctx, ld, args, 1 /* verbosity */);
    dd.check_mtime (tp);
  }

  t.mtime (system_clock::now ());
  return target_state::changed;
}

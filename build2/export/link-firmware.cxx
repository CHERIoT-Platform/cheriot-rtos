// c++ 1

recipe
apply (action a, target& xt, match_extra& me) const override
{
  context& ctx (xt.ctx);

  const scope& bs (xt.base_scope ());
  const scope& rs (*bs.root_scope ());

  // Inject pattern's group members.
  //
  pattern->apply_group_members (a, xt, bs, me);

  file& t (xt.as<file> ());                // firmware()
  file& dt (t.adhoc_member->as<file> ());  // dump()
  file& rt (dt.adhoc_member->as<file> ()); // json()

  // Derive target paths.
  //
  t.derive_path ();
  dt.derive_path ();
  rt.derive_path ();

  // Match prerequisites.
  //
  inject_fsdir (a, t);

  // This is essentially match_prerequisite_members() with the
  // pattern->apply_prerequisites() call worked in between.
  //

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

    const target& pt (p.search (t));

    // Re-create the clean semantics as in match_prerequisite_members().
    //
    if (a.operation () == clean_id && !pt.in (rs))
      continue;

    pts.push_back (prerequisite_target (&pt, pi));
  }

  // Inject pattern's prerequisites.
  //
  pattern->apply_prerequisites (a, t, bs, me);

  // Start asynchronous matching of prerequisites. Wait with unlocked phase to
  // allow phase switching.
  //
  wait_guard wg (ctx, ctx.count_busy (), t[a].task_count, true);

  for (const prerequisite_target& pt: pts)
    match_async (a, *pt.target, ctx.count_busy (), t[a].task_count);

  wg.wait ();

  // Finish matching.
  //
  for (const prerequisite_target& pt: pts)
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
  tracer trace ("link_firmware::perform_update");

  context& ctx (xt.ctx);

  const scope& bs (xt.base_scope ());
  const scope& rs (*bs.root_scope ());

  const file& t (xt.as<file> ());                // firmware()
  const file& dt (t.adhoc_member->as<file> ());  // dump()
  const file& rt (dt.adhoc_member->as<file> ()); // json()

  const path& tp (t.path ());
  const path& dtp (dt.path ());
  const path& rtp (rt.path ());

  process_path ld (run_search (cast<path> (rs["ld"])));
  process_path objdump (run_search (cast<path> (rs["objdump"])));

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
    "--script", find_if (pts.begin (), pts.end (),
                         [t = string ("ldscript")] (const prerequisite_target& pt)
                         {
                           return pt.target->type ().name == t;
                         })->target->as<file> ().path ().string ().c_str (),
    "--compartment-report", rtp.string ().c_str (),
    "-o", tp.string ().c_str (),
  };

  // Append all the input paths.
  //
  {
    // Append prerequisite libraries, recursively.
    //
    auto append_libs = [a, &args] (const target& t,
                                   const auto& append_libs) -> void
    {
      for (const prerequisite_target& pt: t.prerequisite_targets[a])
      {
        const target& p (*pt.target);
        string t (p.type ().name);

        if (t == "library" || t == "privileged_library")
        {
          args.push_back (p.as<file> ().path ().string ().c_str ());
          append_libs (p, append_libs);
        }
      }
    };

    for (const prerequisite_target& pt: pts)
    {
      const target& p (*pt.target);
      string t (p.type ().name);

      if (t == "obje" ||
          t == "library"     || t == "privileged_library" ||
          t == "compartment" || t == "privileged_compartment")
      {
        args.push_back (p.as<file> ().path ().string ().c_str ());

        if (t != "obje")
          append_libs (p, append_libs);
      }
    }
  }

  // Hash the command line and and compare with depdb.
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
    print_diag ("ld", {t.key (), dt.key (), rt.key ()});
  else if (verb >= 2)
    print_process (args);

  if (!ctx.dry_run)
  {
    run (ctx, ld, args, 1 /* verbosity */);
    dd.check_mtime (tp);
  }

  // Generate the dump.
  //
  args = {
    objdump.recall_string (),
    "-glxsdrS",
    "--demangle",
    tp.string ().c_str (),
    nullptr};

  if (verb >= 2)
    print_process (args);

  if (!ctx.dry_run)
  {
    // llvm-objdump doesn't have a way to save the output to a file so we have
    // to jump through a few hoops to redirect the stdout.
    //
    auto_fd o (fdopen (dtp,
                       fdopen_mode::out |
                       fdopen_mode::create |
                       fdopen_mode::truncate));

    process pr (run_start (objdump,
                           args,
                           0                       /* stdin  */,
                           o.get ()                /* stdout */,
                           diag_buffer::pipe (ctx) /* stderr */));
    diag_buffer dbuf (ctx, args[0], pr);
    dbuf.read ();
    run_finish (dbuf, args, pr, 1 /* verbosity */);
  }

  t.mtime (system_clock::now ());
  return target_state::changed;
}

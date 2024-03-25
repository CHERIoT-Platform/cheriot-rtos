// c++ 1 ---

// For vocabulary types (string, path, vector), see <libbuild2/types.hxx>.
// For standard utilities (move(), to_string()) see <libbuild2/utility.hxx>.
//
// Include extra headers, define global functions, etc., before the `---`
// separator.

namespace build2
{
  static inline const target_type&
  find_tt (const scope& rs, const char* n)
  {
    const target_type* tt (rs.find_target_type (n));
    assert (tt != nullptr); // Should be known.
    return *tt;
  }
}

---

// namespace build2
// {
//   class rule: public simple_rule
//   {

virtual recipe
apply (action a, target& xt, match_extra& me) const override
{
  context& ctx (xt.ctx);

  const scope& bs (xt.base_scope ());
  const scope& rs (*bs.root_scope ());

  const dir_path& sdk (cast<dir_path> (rs["sdk"]));
  const json_object& board (cast<json_object> (rs["board"]));

  const json_array* threads (cast_null<json_array> (xt["threads"]));
  if (threads == nullptr || threads->array.empty ())
    fail << "no 'threads' variable set on " << xt;

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

  // Search and match prerequisites.
  //
  inject_fsdir (a, t, false /* match */);

  // This is essentially the standard match_prerequisite_members() with some
  // dependency synthesis and the pattern->apply_prerequisites() call worked
  // in between.
  //
  auto& pts (t.prerequisite_targets[a]);

  // Search and add target's prerequisites.
  //
  search_prerequisite_members (a, t);

  // Inject pattern's prerequisites.
  //
  pattern->apply_prerequisites (a, t, bs, me);

  // Synthesize the dependency on the firmware-specific scheduler compartment.
  //
  // Use `.` instead of, say, `-` or `_` as a separator between the firmware
  // name and `scheduler` to minimize the chance of clashes between several
  // firmware (though seeing that this is inside cheriot-rtos/core/, it's
  // unclear what we could clash with).
  //
  // We need to synthesize the equivalent of this (`.` is escaped as `..`):
  //
  // cheriot-rtos/core/
  // {
  //   privileged_compartment{hello_world..scheduler}: scheduler/obje{hello_world..main} fsdir{.}
  //
  //   scheduler/obje{hello_world..main}: $sdk/core/scheduler/cxx{main} fsdir{scheduler/}
  //   {
  //     cc.poptions += -DCONFIG_THREADS_NUM=1
  //   }
  // }
  //
  {
    const target_type& com_tt (find_tt (rs, "privileged_compartment"));
    const target_type& obj_tt (find_tt (rs, "obje"));
    const target_type& cxx_tt (find_tt (rs, "cxx"));

    dir_path d (rs.out_path ()            /
                dir_path ("cheriot-rtos") /
                dir_path ("core")         /
                dir_path ("scheduler"));

    // scheduler/obje{hello_world..main}: $sdk/core/scheduler/cxx{main}
    //
    pair<target&, ulock> obj_tl (
      search_new_locked (ctx,
                         obj_tt,
                         d,
                         dir_path (), // out (always in out)
                         t.name + ".main"));

    // Assume this is already done if the target exists (e.g., operation
    // batch).
    //
    if (obj_tl.second.owns_lock ())
    {
      prerequisites obj_ps {prerequisite (fsdir::static_type,
                                          d,
                                          dir_path (),
                                          string (),
                                          string (),
                                          rs)};
      obj_ps.push_back (
        prerequisite (cxx_tt,
                      sdk / dir_path ("core") / dir_path ("scheduler"),
                      dir_path (),
                      "main",
                      "cc",
                      rs));

      obj_tl.first.prerequisites (move (obj_ps));

      cast<strings> (obj_tl.first.append_locked ("cc.poptions")).push_back (
        "-DCONFIG_THREADS_NUM=" + to_string (threads->array.size ()));

      obj_tl.second.unlock ();
    }

    d.make_directory (); // Back to core/.

    // privileged_compartment{hello_world..scheduler}: scheduler/obje{hello_world..main}
    //
    pair<target&, ulock> com_tl (
      search_new_locked (ctx,
                         com_tt,
                         d,
                         dir_path (),
                         t.name + ".scheduler"));


    if (com_tl.second.owns_lock ())
    {
      prerequisites com_ps {prerequisite (fsdir::static_type,
                                          d,
                                          dir_path (),
                                          string (),
                                          string (),
                                          rs)};
      com_ps.push_back (prerequisite (obj_tl.first, true /* locked */));
      com_tl.first.prerequisites (move (com_ps));
      com_tl.second.unlock ();
    }

    pts.push_back (com_tl.first);
  }

  // Synthesize the dependency on the firmware-specific linker script.
  //
  // We need to synthesize the equivalent of this (`.` is escaped as `..`):
  //
  // cheriot-rtos/core/
  // {
  //   ldscript{hello_world..firmware}: $sdk/in{firmware.ldscript.in}
  //   {
  //     mmio = ...
  //     ...
  //   }
  //
  {
    // Pre-calculate the substitution variables outside the lock.
    //
    string mmio;
    try
    {
      uint64_t mmio_start (0xffffffff), mmio_end (0);
      uint64_t heap_end (board.at ("heap").at ("end").as_uint64 ());

      for (const json_member& m: board.at ("devices").as_object ())
      {
        const json_value& v (m.value);

        uint64_t s (v.at ("start").as_uint64 ());
        uint64_t e;
        if (const json_value* p = v.find ("end"))
          e = p->as_uint64 ();
        else
          e = s + v.at ("length").as_uint64 ();

        if (mmio_start > s) mmio_start = s;
        if (mmio_end < e) mmio_end = e;

        mmio += "__export_mem_" + m.name + " = " + to_string (s, 16) + ";\n";
        mmio += "__export_mem_" + m.name + "_end = " + to_string (e, 16) + ";\n";
      }

      mmio.insert (0, "__mmio_region_start = " + to_string (mmio_start, 16) + ";\n");
      mmio += "__mmio_region_end = " + to_string (mmio_end, 16) + ";\n";

      mmio += "__export_mem_heap_end = " + to_string (heap_end, 16) + ";\n";
    }
    catch (const std::exception& e)
    {
      fail << "invalid board json: " << e.what ();
    }

    const target_type& ls_tt (find_tt (rs, "ldscript"));
    const target_type& in_tt (find_tt (rs, "in"));

    dir_path d (rs.out_path () / dir_path ("cheriot-rtos") / dir_path ("core"));

    pair<target&, ulock> ls_tl (
      search_new_locked (ctx,
                         ls_tt,
                         d,
                         dir_path (),
                         t.name + ".firmware"));

    // Assume this is already done if the target exists (e.g., operation
    // batch).
    //
    if (ls_tl.second.owns_lock ())
    {
      prerequisites ls_ps {prerequisite (fsdir::static_type,
                                         d,
                                         dir_path (),
                                         string (),
                                         string (),
                                         rs)};

      ls_ps.push_back (prerequisite (in_tt,
                                     sdk,
                                     dir_path (),
                                     "firmware.ldscript",
                                     // Until we've implemented this fully, use
                                     // a partially expanded version.
                                     "expanded",// "in"
                                     rs));

      target& ls (ls_tl.first);

      ls.prerequisites (move (ls_ps));

      // Configure the alternative substitution symbol.
      //
      ls.assign ("in.symbol") = "@";

      // Set substitution variables (e.g., @mmio@, etc).
      //
      // NOTE: these variables must be pre-entered in rules.build2!
      //
      ls.assign ("mmio") = move (mmio);

      ls_tl.second.unlock ();
    }

    pts.push_back (ls_tl.first);
  }

  // Finally match all the prerequisite members.
  //
  match_members (a, t, pts);

  switch (a)
  {
  case perform_update_id:  return perform_update;
  case perform_clean_id:   return perform_clean_depdb;
  default:                 return noop_recipe; // Configure update.
  }
}

static target_state
perform_update (action a, const target& xt)
{
  tracer trace ("link_firmware::perform_update");

  context& ctx (xt.ctx);

  const scope& bs (xt.base_scope ());
  const scope& rs (*bs.root_scope ());

  const file& t (xt.as<file> ());                // firmware{}
  const file& dt (t.adhoc_member->as<file> ());  // dump{}
  const file& rt (dt.adhoc_member->as<file> ()); // json{}

  const path& tp (t.path ());

  process_path ld (run_search (cast<path> (rs["ld"])));
  process_path objdump (run_search (cast<path> (rs["objdump"])));

  timestamp mt (t.load_mtime ());
  optional<target_state> ps (execute_prerequisites (a, t, mt));

  auto& pts (t.prerequisite_targets[a]);

  const file& ls ( // ldscript{} prerequisite
    find_if (pts.begin (), pts.end (),
             [&tt = find_tt (rs, "ldscript")] (const prerequisite_target& pt)
             {
               return pt.target != nullptr && pt.target->type ().is_a (tt);
             })->target->as<file> ());

  // Start building the command line. While we don't yet know whether we will
  // really need it, the easiest is to hash it and find out.
  //
  cstrings args {
    ld.recall_string (),
    "--relax",
    "--script", ls.path ().string ().c_str (),
    "--compartment-report", rt.path ().string ().c_str (),
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
        if (pt.target == nullptr) // Skip "holes".
          continue;

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
      if (pt.target == nullptr) // Skip "holes".
        continue;

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

  // @@ TODO: change-track the ld/objdump versions.
  //
  depdb dd (tp + ".d");

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
    auto_fd o (fdopen (dt.path (),
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

//   }; // class rule
// } // namespace build2

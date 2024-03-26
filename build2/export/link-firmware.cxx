// c++ 1 //---

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

//---


// namespace build2
// {
//   class rule: public simple_rule
//   {

static bool is_privileged_library(const target &t)
{
    return t.type().name == string("privileged_library");
}

static bool is_normal_library(const target &t)
{
    return t.type().name == string("library") || t.type().name == string("privileged_library");
}

static bool is_privileged_compartment(const target &t)
{
    return t.type().name == string("privileged_compartment");
}

static bool is_normal_compartment(const target &t)
{
    return t.type().name == string("compartment");

}

static bool is_library(const target &t)
{
    return is_normal_library(t) || is_privileged_library(t);
}

static bool is_compartment(const target &t)
{
    return is_normal_compartment(t) || is_privileged_compartment(t);
}

/**
 * Generate the trusted stacks section of the linker script.
 */
string thread_trusted_stacks(const json_array &threads) const
{
    // FIXME: This should be computed based on the enabled features.
    uint32_t loaderTrustedStackSize = 192;
    string trustedStacks =
        "\n\t. = ALIGN(8);"
        "\n\t.loader_trusted_stack : CAPALIGN"
        "\n\t{"
        "\n\t\tbootTStack = .;"
        "\n\t\t. += " + to_string(loaderTrustedStackSize) + ";"
        "\n\t}\n";
    int threadID = 1;
    try
    {
        for (auto &thread : threads.array)
        {
            string threadIDStr = to_string(threadID);
            trustedStacks +=
                "\n\t. = ALIGN(8);"
                "\n\t.thread_trusted_stack_" + threadIDStr + " : CAPALIGN"
                "\n\t{"
                "\n\t\t.thread_" + threadIDStr + "_trusted_stack_start = .;"
                "\n\t\t. += " + to_string(loaderTrustedStackSize + (thread.at("trusted_stack_frames").as_uint64() * 24)) + ";"
                "\n\t\t.thread_" + threadIDStr + "_trusted_stack_end = .;"
                "\n\t}\n";
            threadID++;
        }
    }
    catch (const std::exception &e)
    {
        fail << "invalid threads json while extracting trusted stacks: " << e.what();
    }
    return trustedStacks;
}

/**
 * Generate the thread stacks section of the linker script.
 */
string thread_stacks(const json_array &threads) const
{
    string threadStacks =
        "\n\t. = ALIGN(16);"
        "\n\t.loader_stack : CAPALIGN"
        "\n\t{"
        "\n\t\tbootStack = .;"
        "\n\t\t. += 1024;"
        "\n\t}\n";

    int threadID = 1;
    try
    {
        for (auto &thread : threads.array)
        {
            string threadIDStr = to_string(threadID);
            threadStacks +=
                "\n\t. = ALIGN(16);"
                "\n\t.thread_stack_" + threadIDStr + " : CAPALIGN"
                "\n\t{"
                "\n\t\t.thread_" + threadIDStr + "_stack_start = .;"
                "\n\t\t. += " + to_string(thread.at("stack_size").as_uint64()) + ";"
                "\n\t\t.thread_" + threadIDStr + "_stack_end = .;"
                "\n\t}\n";
            threadID++;
        }
    }
    catch (const std::exception &e)
    {
        fail << "invalid threads json while extracting thread stacks: " << e.what();
    }
    return threadStacks;
}

string compartment_exports(std::vector<const target*> &compartments) const
{
    string exports;
    for (auto compartment : compartments)
    {
      exports +=
       "\n\t\t." + compartment->name + "_export_table = ALIGN(8);"
       "\n\t\t" + compartment->as<file>().path().string() + "(.compartment_export_table);"
       "\n\t\t." + compartment->name + "_export_table_end = .;\n";
    }
    return exports;
}

string compartment_pccs(std::vector<const target*> &compartments) const
{
    string pccs;
    for (auto compartment : compartments)
    {
      string name = compartment->name;
      string object = compartment->as<file>().path().string();
      pccs +=
        "\n\t." + name + "_code : CAPALIGN"
        "\n\t{"
        "\n\t\t." + name + "_code_start = .;"
        "\n\t\t" + object + "(.compartment_import_table);"
        "\n\t\t." + name + "_imports_end = .;"
        "\n\t\t" + object + "(.text);"
        "\n\t\t" + object + "(.init_array);"
        "\n\t\t" + object + "(.rodata);"
        "\n\t\t. = ALIGN(8);"
        "\n\t}\n";
    }
    return pccs;
}

string compartment_cgps(std::vector<const target*> &compartments) const
{
    string cgps;
    for (auto compartment : compartments)
    {
      string name = compartment->name;
      string object = compartment->as<file>().path().string();
      cgps +=
        "\n\t." + name + "_globals : CAPALIGN"
        "\n\t{"
        "\n\t\t." + name + "_globals = .;"
        "\n\t\t" + object + "(.data);"
        "\n\t\t." + name + "_bss_start = .;"
        "\n\t\t" + object + "(.bss)"
        "\n\t}\n";

    }
    return cgps;
}

string compartment_sealed_objects(std::vector<const target*> &compartments) const
{
    string sealedObjects;
    for (auto compartment : compartments)
    {
      string name = compartment->name;
      string object = compartment->as<file>().path().string();
      sealedObjects +=
        "\n\t\t." + name + "_sealed_objects_start = .;"
        "\n\t\t" + object + "(.sealed_objects);\n\t\t." + name + "_sealed_objects_end = .;";
    }
    return sealedObjects;
}

string compartment_cap_relocs(std::vector<const target*> &compartments) const
{
    string capRelocs;
    for (auto compartment : compartments)
    {
      string name = compartment->name;
      string object = compartment->as<file>().path().string();
      capRelocs +=
        "\n\t\t." + name + "_cap_relocs_start = .;"
        "\n\t\t" + object + "(__cap_relocs);\n\t\t." + name + "_cap_relocs_end = .;";
    }
    return capRelocs;
}

string compartment_headers(std::vector<const target*> &compartments, bool isCompartment) const
{
    string headers;
    for (auto compartment : compartments)
    {
      string name = compartment->name;
      headers +=
        "\n\t\tLONG(." + name + "_code_start);"
        "\n\t\tSHORT((SIZEOF(." + name + "_code) + 7) / 8);"
        "\n\t\tSHORT(." + name + "_imports_end - ." + name + "_code_start);"
        "\n\t\tLONG(." + name + "_export_table);"
        "\n\t\tSHORT(." + name + "_export_table_end - ." + name + "_export_table);";
      if (isCompartment)
      {
        headers +=
          "\n\t\tLONG(." + name + "_globals);"
          "\n\t\tSHORT(SIZEOF(." + name + "_globals));"
          "\n\t\tSHORT(." + name + "_bss_start - ." + name + "_globals);";
      }
      else
      {
        headers +=
          "\n\t\tLONG(0);"
          "\n\t\tSHORT(0);"
          "\n\t\tSHORT(0);";
      }
      headers +=
        "\n\t\tLONG(." + name + "_cap_relocs_start);"
        "\n\t\tSHORT(." + name + "_cap_relocs_end - ." + name + "_cap_relocs_start);"
        "\n\t\tLONG(." + name + "_sealed_objects_start);"
        "\n\t\tSHORT(." + name + "_sealed_objects_end - ." + name + "_sealed_objects_start);\n";
    }
    return headers;
}

recipe
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

  // Next match all the prerequisite members.
  //
  // Note: do this before synthesizing the dependency on the linker script to
  // resolve all the prerequisite compartments and libraries recursively.
  //
  match_members (a, t, pts);

  // Collect all of the libraries and compartments that we depend on for
  // insertion into the linker script.
  std::vector<const target*> libraries;
  std::vector<const target*> compartments;
  std::vector<const target*> privilegedLibraries;
  std::vector<const target*> privilegedCompartments;
  {
    // Helper to find the vector that a target should be inserted into.
    auto vectorForTarget = [&](const target &t) -> std::optional<std::vector<const target*>*> {
      if (is_normal_library(t))
      {
        return &libraries;
      }
      if (is_normal_compartment(t))
      {
        return &compartments;
      }
      if (is_privileged_library(t))
      {
        return &privilegedLibraries;
      }
      if (is_privileged_compartment(t))
      {
        return &privilegedCompartments;
      }
      return std::nullopt;
    };
    // Append prerequisite libraries, recursively.
    //
    auto append_libs = [&] (const target& t,
                            const auto& append_libs) -> void
    {
      for (const prerequisite_target& pt: t.prerequisite_targets[a])
      {
        if (pt.target == nullptr) // Skip "holes".
          continue;

        const target& p (*pt.target);
        string t (p.type ().name);
        if (auto vec = vectorForTarget(p))
        {
          (*vec)->push_back(&p);
          append_libs(p, append_libs);
        }
      }
    };

    for (const prerequisite_target& pt: pts)
    {
      if (pt.target == nullptr) // Skip "holes".
        continue;

      const target& p (*pt.target);
      string t (p.type ().name);
      if (auto vec = vectorForTarget(p))
      {
        (*vec)->push_back(&p);
        append_libs(p, append_libs);
      }
    }
  }

  // Finally synthesize the dependency on the firmware-specific linker script
  // and match it to a rule.
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
      fail << "invalid board json while extracting MMIO regions: " << e.what ();
    }

    string code_start;
    try
    {
      code_start = to_string(board.at("instruction_memory").at("start").as_uint64(), 16);
    }
    catch (const std::exception& e)
    {
      fail << "invalid board json while extracting code start address: " << e.what ();
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
      ls.assign ("code_start") = code_start;
      ls.assign ("thread_trusted_stacks") = thread_trusted_stacks(*threads);
      ls.assign ("thread_stacks") = thread_stacks(*threads);
      ls.assign ("compartment_exports") = compartment_exports(compartments);
      ls.assign ("pcc_ld") = compartment_pccs(libraries) + compartment_pccs(compartments);
      ls.assign ("gdc_ld") = compartment_cgps(compartments);
      ls.assign ("sealed_objects") = compartment_sealed_objects(libraries) + compartment_sealed_objects(compartments);
      ls.assign ("cap_relocs") = compartment_cap_relocs(libraries) + compartment_cap_relocs(compartments);
      ls.assign ("library_count") = to_string(libraries.size());
      ls.assign ("compartment_count") = to_string(compartments.size());
      ls.assign ("compartment_headers") = compartment_headers(libraries, false) + compartment_headers(compartments, true);
      // Get the heap start if it exists, otherwise default to "." (current location).
      string heap_start = ".";
      try
      {
        heap_start = to_string(board.at("heap").at("start").as_uint64(), 16);
      }
      catch(...) {}
      ls.assign ("heap_start") = heap_start;

      ls_tl.second.unlock ();
    }

    pts.push_back (ls_tl.first);
    match_sync (a, ls_tl.first);
  }

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

        if (is_library(p) || is_compartment(p))
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

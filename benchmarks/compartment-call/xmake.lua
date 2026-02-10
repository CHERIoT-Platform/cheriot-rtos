-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT cross-compartment benchmark");
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib"))

option("board")
    set_default("sail")

option("metric")
    set_default("rdcycle")
    set_values("rdcycle", "rdinstret", "rd_lsu_stalls", "rd_ifetch_stalls")

library("libcallee")
    add_files("libcallee.cc")
    add_defines("METRIC=" .. tostring(get_config("metric")))

compartment("callee")
    add_files("callee.cc")
    add_defines("METRIC=" .. tostring(get_config("metric")))

compartment("callee_seh")
    add_files("callee_seh.cc")
	add_deps("unwind_error_handler")

compartment("callee_ueh")
    add_files("callee_ueh.cc")

debugOption("caller");
compartment("caller")
    -- Allow allocating an effectively unbounded amount of memory (more than exists)
    add_rules("cheriot.component-debug")
    add_defines("BOARD=" .. tostring(get_config("board")))
    add_defines("METRIC=" .. tostring(get_config("metric")))
    add_files("caller.cc")

-- Firmware image for the example.
firmware("compartment-call-benchmark")
    add_deps("crt", "freestanding", "atomic", "stdio", "string", "debug")
    add_deps("caller", "callee", "libcallee", "callee_seh", "callee_ueh")
    on_load(function(target)
        target:values_set("threads", {
            {
                compartment = "caller",
                priority = 1,
                entry_point = "run",
                stack_size = 0x100,
                trusted_stack_frames = 3
            },
            {
                compartment = "caller",
                priority = 1,
                entry_point = "run",
                stack_size = 0x200,
                trusted_stack_frames = 3
            },
            {
                compartment = "caller",
                priority = 1,
                entry_point = "run",
                stack_size = 0x400,
                trusted_stack_frames = 3
            },
            {
                compartment = "caller",
                priority = 1,
                entry_point = "run",
                stack_size = 0x800,
                trusted_stack_frames = 3
            },
            {
                compartment = "caller",
                priority = 1,
                entry_point = "run",
                stack_size = 0x1000,
                trusted_stack_frames = 3
            }
        }, {expand = false})
    end)

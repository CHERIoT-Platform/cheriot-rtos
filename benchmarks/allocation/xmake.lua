-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT cross-compartment benchmark");
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

debugOption("allocbench");
compartment("allocbench")
    add_deps("crt", "freestanding", "atomic", "stdio", "debug")
    -- Allow allocating an effectively unbounded amount of memory (more than exists)
    add_rules("cheriot.component-debug")
    add_defines("MALLOC_QUOTA=1000000")
    add_defines("BOARD=" .. tostring(get_config("board")))
    add_files("alloc.cc")

-- Firmware image for the example.
firmware("allocator-benchmark")
    add_deps("allocbench")
    on_load(function(target)
        target:values_set("threads", {
            {
                compartment = "allocbench",
                priority = 1,
                entry_point = "run",
                stack_size = 0x400,
                trusted_stack_frames = 4
            },
        }, {expand = false})
    end)

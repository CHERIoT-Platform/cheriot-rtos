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

compartment("callee")
    add_files("callee.cc")

compartment("caller")
    add_defines("BOARD=" .. tostring(get_config("board")))
    add_files("caller.cc")

-- Firmware image for the example.
firmware("compartment-call-benchmark")
    add_deps("crt", "freestanding", "atomic", "stdio", "string")
    add_deps("caller", "callee")
    on_load(function(target)
        target:values_set("board", "$(board)")
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

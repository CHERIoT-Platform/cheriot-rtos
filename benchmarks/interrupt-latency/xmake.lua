-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT interrupt-latency benchmark");
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/crt"))

option("board")
    set_default("sail")

debugOption("interrupt_bench");
compartment("interrupt_bench")
    -- Allow allocating an effectively unbounded amount of memory (more than exists)
    add_rules("cherimcu.component-debug")
    add_defines("BOARD=" .. tostring(get_config("board")))
    add_files("interrupt_bench.cc")

-- Firmware image for the example.
firmware("interrupt-benchmark")
    add_deps("crt", "freestanding", "atomic")
    add_deps("interrupt_bench")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "interrupt_bench",
                priority = 2,
                entry_point = "entry_high_priority",
                stack_size = 0x1000,
                trusted_stack_frames = 4
            },
            {
                compartment = "interrupt_bench",
                priority = 2,
                entry_point = "entry_high_priority",
                stack_size = 0x800,
                trusted_stack_frames = 4
            },
            {
                compartment = "interrupt_bench",
                priority = 2,
                entry_point = "entry_high_priority",
                stack_size = 0x400,
                trusted_stack_frames = 4
            },
            {
                compartment = "interrupt_bench",
                priority = 2,
                entry_point = "entry_high_priority",
                stack_size = 0x200,
                trusted_stack_frames = 4
            },
            {
                compartment = "interrupt_bench",
                priority = 1,
                entry_point = "entry_low_priority",
                stack_size = 0x100,
                trusted_stack_frames = 4
            },
        }, {expand = false})
    end)

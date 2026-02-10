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

option("irq-source")
    set_default("ibex_revoker")
    set_values("ibex_revoker", "sunburst_uart1")

option("metric")
    set_default("rdcycle")
    set_values("rdcycle", "rdinstret", "rd_lsu_stalls", "rd_ifetch_stalls")

debugOption("interrupt_bench");
compartment("interrupt_bench")
    add_deps("crt", "freestanding", "stdio", "debug")
    -- Allow allocating an effectively unbounded amount of memory (more than exists)
    add_rules("cheriot.component-debug")
    add_defines("BOARD=" .. tostring(get_config("board")))
    add_defines("METRIC=" .. tostring(get_config("metric")))
    add_defines("IRQ_SOURCE_" .. tostring(get_config("irq-source")))
    add_files("interrupt_bench.cc")

-- Firmware image for the example.
firmware("interrupt-benchmark")
    add_deps("interrupt_bench")
    on_load(function(target)
        target:values_set("threads", {
            {
                compartment = "interrupt_bench",
                priority = 2,
                entry_point = "entry_high_priority",
                stack_size = 0x400,
                trusted_stack_frames = 4
            },
            {
                compartment = "interrupt_bench",
                priority = 1,
                entry_point = "entry_low_priority",
                stack_size = 0x400,
                trusted_stack_frames = 4
            },
        }, {expand = false})
    end)

set_project("RTOS scheduler basic functionality test")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("ibex-safe-simulator")

compartment("top")
    add_files("top.cc")

firmware("keep_ticking")
    add_deps("freestanding", "debug")
    add_deps("top")
    on_load(function(target)
        target:values_set("threads", {
            {
                compartment = "top",
                priority = 0,
                entry_point = "entry1",
                stack_size = 0x300,
                trusted_stack_frames = 1
            },
            {
                compartment = "top",
                priority = 0,
                entry_point = "entry2",
                stack_size = 0x300,
                trusted_stack_frames = 1
            }
        }, {expand = false})

    end)

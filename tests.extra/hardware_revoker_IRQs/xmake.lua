set_project("Hardware Revoker IRQ Basic Functionality Test")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("ibex-safe-simulator")

compartment("top")
    add_files("top.cc")

firmware("top_compartment")
    add_deps("freestanding", "debug")
    add_deps("top")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "top",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x400,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)

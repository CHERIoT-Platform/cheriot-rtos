set_project("CHERIoT Scheduler IRQ Exception PoC")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

compartment("helper")
    add_files("helper.cc")

compartment("top")
    add_files("top.cc")

firmware("top_compartment")
    add_deps("freestanding", "debug")
    add_deps("top", "helper")
    on_load(function(target)
        target:values_set("threads", {
            {
                compartment = "top",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x200,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)

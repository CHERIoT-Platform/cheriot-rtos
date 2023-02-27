set_project("CHERIoT memory safety")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/crt"))

option("board")
    set_default("sail")

compartment("memory_safety_inner")
    add_files("memory_safety_inner.cc")

compartment("memory_safety_runner")
    add_files("memory_safety_runner.cc")

-- Firmware image for the example.
firmware("memory_safety")
    add_deps("crt", "freestanding", "atomic_fixed")
    add_deps("memory_safety_runner", "memory_safety_inner")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "memory_safety_runner",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x400,
                trusted_stack_frames = 5
            }
        }, {expand = false})
    end)

-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Compartmentalised hello world (more secure)")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib"))

option("board")
    set_default("sail")

compartment("uart")
    add_files("uart.cc")

compartment("hello")
    add_files("hello.cc")

-- Firmware image for the example.
firmware("error_handling")
    -- Both compartments require memcpy
    add_deps("freestanding", "debug")
    add_deps("hello", "uart")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "hello",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x400,
                trusted_stack_frames = 2
            }
        }, {expand = false})
    end)

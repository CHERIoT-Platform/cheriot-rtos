-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Compartmentalised hello world")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/crt"))


option("board")
    set_default("sail")

compartment("uart")
    add_files("uart.cc")

compartment("hello")
    add_files("hello.cc")

-- Firmware image for the example.
firmware("hello_compartment")
    add_deps("crt", "freestanding")
    add_deps("hello", "uart")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "hello",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x200,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)

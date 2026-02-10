-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Hello World")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

compartment("hello")
    add_files("hello.cc")

-- Firmware image for the example.
firmware("javascript")
    add_deps("microvium", "debug")
    add_deps("hello")
    on_load(function(target)
        target:values_set("threads", {
            {
                compartment = "hello",
                priority = 1,
                entry_point = "say_hello",
                stack_size = 0x800,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)

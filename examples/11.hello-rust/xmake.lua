-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Hello World")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sail")
option_end()

compartment("hello")
    -- memcpy
	add_rules("cheriot.rust")
    add_deps("freestanding", "debug", "stdio")
    add_files("hello.cc")
	add_files("rust.rs")

    
-- Firmware image for the example.
firmware("hello_world")
    add_deps("hello")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "hello",
                priority = 1,
                entry_point = "say_hello",
                stack_size = 0x1F00,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)

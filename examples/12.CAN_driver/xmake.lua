-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("Sonata MCP251XFD Example")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sonata-1.1")

compartment("main_comp")
    -- memcpy
    add_deps("freestanding", "debug")
    add_files("example.cc")
    add_files("interface.cc")

-- Firmware image for the example.
firmware("mcp251xfd")
    add_deps("main_comp")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "main_comp",
                priority = 1,
                entry_point = "main_entry",
                stack_size = 0x800,
                trusted_stack_frames = 1
            }
        }, {expand = false})
    end)

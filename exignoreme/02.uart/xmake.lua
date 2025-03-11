-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("Sonata UART Example")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sonata")

-- compartment("main_comp")
--     -- memcpy
--     add_deps("freestanding", "debug", "stdio")
--     add_files("example.cc", "modem.cc")

compartment("uart")
    -- memcpy
    add_deps("freestanding", "debug", "stdio")
    add_files("uart.cc", "modem.cc")

-- Firmware image for the example.
firmware("basic_uart")
    -- add_deps("main_comp")
    add_deps("uart")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            -- {
            --     compartment = "main_comp",
            --     priority = 1,
            --     entry_point = "main_entry",
            --     stack_size = 0x800,
            --     trusted_stack_frames = 4
            -- },
            {
                compartment = "uart",
                priority = 1,
                entry_point = "uart_entry",
                stack_size = 0x800,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)

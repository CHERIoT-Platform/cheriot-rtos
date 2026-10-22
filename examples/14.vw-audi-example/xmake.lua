-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("Sonata VW-Audi CAN Example")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sonata-1.1")

compartment("main_comp")
    add_files("example.cc", "driver/interface.cc")

compartment("uart")
    add_files("uart.cc", "modem.cc")

-- Firmware image for the example.
firmware("vag_can")
    add_deps("freestanding", "debug", "stdio", "message_queue")
    add_deps("main_comp", "uart")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "main_comp",
                priority = 1,
                entry_point = "main_entry",
                stack_size = 0xA00,
                trusted_stack_frames = 5
            },
            {
                compartment = "uart",
                priority = 1,
                entry_point = "uart_entry",
                stack_size = 0x800,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)

-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Compartmentalised hello world (more secure)")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

compartment("caesar")
    -- This compartment uses C++ thread-safe static initialisation and so
    -- depends on the C++ runtime.
    add_files("caesar_cypher.cc")

compartment("entry")
    add_files("entry.cc")

compartment("producer")
    add_files("producer.cc")
compartment("consumer")
    add_files("consumer.cc")

-- Firmware image for the example.
firmware("audit")
    -- Both compartments require memcpy
    add_deps("freestanding", "debug", "string")
    add_deps("entry", "caesar")
    add_deps("producer", "consumer")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "entry",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x400,
                trusted_stack_frames = 3
            }
        }, {expand = false})
    end)

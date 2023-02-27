-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Compartmentalised hello world (more secure)")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/crt"))

option("board")
    set_default("sail")

compartment("producer")
    add_files("producer.cc")

compartment("consumer")
    add_files("consumer.cc")

-- Firmware image for the example.
firmware("producer-consumer")
    add_deps("crt", "freestanding", "atomic_fixed")
    add_deps("producer", "consumer")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "producer",
                priority = 1,
                entry_point = "run",
                stack_size = 0x400,
                trusted_stack_frames = 3
            },
            {
                compartment = "consumer",
                priority = 1,
                entry_point = "run",
                stack_size = 0x400,
                trusted_stack_frames = 3
            }
        }, {expand = false})
    end)

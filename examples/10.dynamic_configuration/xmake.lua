-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

-- Contributed by Configured Things Ltd

set_project("CHERIoT Compartmentalised Config")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib"))

option("board")
    set_default("ibex-safe-simulator")

-- Configuration Broker
compartment("config_broker")
    add_files("config_broker.cc")

-- Configurtaion Source
compartment("config_source")
    add_files("config_source.cc")

-- Compartments to be configured
compartment("comp1")
    add_files("comp1.cc")
compartment("comp2")
    add_files("comp2.cc")
compartment("comp3")
    add_files("comp3.cc")

-- Valdation sandpit
compartment("validator")
    add_files("validator.cc")

-- Firmware image for the example.
firmware("compartment_config")
    -- Both compartments require memcpy
    add_deps("freestanding", "debug")
    add_deps("config_source")
    add_deps("config_broker")
    add_deps("comp1", "comp2", "comp3")
    add_deps("validator")
    add_deps("string")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "config_broker",
                priority = 4,
                entry_point = "init",
                stack_size = 0x400,
                trusted_stack_frames = 3
            },
            {
                compartment = "config_source",
                priority = 2,
                entry_point = "init",
                stack_size = 0x400,
                trusted_stack_frames = 2
            },
            {
                compartment = "comp1",
                priority = 3,
                entry_point = "init",
                stack_size = 0x400,
                trusted_stack_frames = 4
            },
            {
                compartment = "comp2",
                priority = 1,
                entry_point = "init",
                stack_size = 0x400,
                trusted_stack_frames = 4
            },
            {
                compartment = "comp3",
                priority = 1,
                entry_point = "init",
                stack_size = 0x400,
                trusted_stack_frames = 4
            },
            {
                compartment = "config_source",
                priority = 2,
                entry_point = "bad_dog",
                stack_size = 0x400,
                trusted_stack_frames = 2
            },
            
        }, {expand = false})
    end)

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

-- Library for configuration data
library("config_data")
    set_default(false)
    add_files("data.cc")     

-- Configuration Broker
debugOption("config_broker");
compartment("config_broker")
    add_rules("cheriot.component-debug")
    add_files("config_broker.cc")

-- Configurtation Publisher
compartment("publisher")
    add_files("publisher.cc")

-- Compartments to be configured
compartment("subscriber1")
    add_files("subscriber1.cc")
compartment("subscriber2")
    add_files("subscriber2.cc")
compartment("subscriber3")
    add_files("subscriber3.cc")

-- Sandbox Compartment
compartment("sandbox")
    add_files("sandbox.cc")

-- Debug options
debugOption("config_broker")

-- Firmware image for the example.
firmware("compartment_config")
    -- Both compartments require memcpy
    add_deps("freestanding", "debug")
    add_deps("config_data")
    add_deps("publisher")
    add_deps("config_broker")
    add_deps("subscriber1")
    add_deps("subscriber2")
    add_deps("subscriber3")
    add_deps("sandbox")
    add_deps("string")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "publisher",
                priority = 2,
                entry_point = "init",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
            {
                compartment = "subscriber1",
                priority = 3,
                entry_point = "init",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
            {
                compartment = "subscriber2",
                priority = 1,
                entry_point = "init",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
            {
                compartment = "subscriber3",
                priority = 1,
                entry_point = "init",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
            {
                compartment = "publisher",
                priority = 2,
                entry_point = "bad_dog",
                stack_size = 0x500,
                trusted_stack_frames = 4
            },
        }, {expand = false})
    end)

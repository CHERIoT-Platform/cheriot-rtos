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
debugOption("config_broker");
compartment("config_broker")
    add_rules("cheriot.component-debug")
    add_files("config_broker.cc")

-- Compartments to be configured
compartment("subscriber1")
    add_files("subscriber1.cc")

-- Debug options
debugOption("config_broker")

-- Firmware image for the example.
firmware("compartment_config")
    -- Both compartments require memcpy
    add_deps("freestanding", "debug")
    add_deps("config_broker")
    add_deps("subscriber1")
    add_deps("string")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "subscriber1",
                priority = 3,
                entry_point = "init",
                stack_size = 0x400,
                trusted_stack_frames = 4
            },
            
        }, {expand = false})
    end)

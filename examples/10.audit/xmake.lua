-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Compartmentalised hello world (more secure)")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

rule("caesar.checks")
    before_build(function (target)
        -- Examples of creating audits:
        -- "query" is the rego query
        -- "module" is the path to an optional rego module
        -- Either can be a string or a table of strings to specify multiple queries and/or modules.
        local audit = {
            query='data.caesar.valid',
            module=path.join(target:scriptdir(), "caesar.rego")
        }
        -- An example of an audit query with multiple rego modules and multiple queries:
        -- local audit = {
        --     query={'data.caesar.valid', 'data.compartment.mmio_allow_list("uart2", {"uart_man"})'},
        --     module={path.join(target:scriptdir(), "caesar.rego"), path.join(target:scriptdir(), "brutus.rego")}
        -- }
        target:set("cheriot.audit", {audit})
    end)

compartment("caesar")
    -- This compartment uses C++ thread-safe static initialisation and so
    -- depends on the C++ runtime.
    add_files("caesar_cypher.cc")

compartment("entry")
    add_files("entry.cc")

compartment("producer")
    add_files("producer.cc")
    add_rules("caesar.checks", {private = false})

compartment("consumer")
    add_files("consumer.cc")

-- Firmware image for the example.
firmware("audit")
    -- Both compartments require memcpy
    add_deps("freestanding", "debug", "string")
    add_deps("entry", "caesar")
    add_deps("producer", "consumer")
    on_load(function(target)
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

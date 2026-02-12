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
        -- local audit = 1234 -- An error!
        -- local audit = 'data.rtos.valid' -- A direct query
        -- local audit = {'data.compartment.mmio_allow_list("uart2", {"uart_man"})'} -- A direct query
        local audit = {'data.caesar.valid', path.join(target:scriptdir(), "caesar.rego")}  -- A query with a rego file
        -- local audit = {  -- Multiple queires (mixed direct queries and rego file queries)
        --     {'data.uart_man.valid("uart1")', path.join(target:scriptdir(), "uart_man.rego")}, 
        --     {'data.compartment.mmio_allow_list("uart2", {"uart_man"})'} 
        -- }
        -- local audit = {  -- Multiple queires (mixed direct queries and rego file queries)
        --     {'data.uart_man.valid("uart1")', path.join(target:scriptdir(), "uart_man.rego")}, 
        --     {'data.uart_man.valid("uart2")', path.join(target:scriptdir(), "uart_man.rego")}
        -- }
        target:set("cheriot.audit", audit)

        -- -- Example of a dynamic query based on the defines set in this compartment
        -- local audit = {}
        -- local defs = target:get("defines")
        -- local search_string = "UART_MANAGE_UART_"
        -- for _,d in ipairs(defs) do
        --     local i, j = string.find(d, search_string)
        --     if(i == 1) then
        --         table.insert(audit, {'data.uart_man.valid("uart'..string.sub(d, j+1, -1)..'")', path.join(target:scriptdir(), "uart_man.rego")})
        --     end
        -- end
        -- target:set("cheriot.audit", audit)
    end)

compartment("caesar")
    -- This compartment uses C++ thread-safe static initialisation and so
    -- depends on the C++ runtime.
    add_files("caesar_cypher.cc")

compartment("entry")
    add_files("entry.cc")

compartment("producer")
    add_files("producer.cc")
    add_rules("caesar.checks", {private = true})

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

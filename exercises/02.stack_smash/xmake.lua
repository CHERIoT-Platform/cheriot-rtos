-- Copyright SCI 2025
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Stack Smash")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

compartment("attacker")
    add_deps("debug")
    add_files("attacker.cc")

compartment("victim")
    add_files("victim.cc")

-- Firmware image for the example.
firmware("stack_smash")
    add_deps("attacker")
    add_deps("victim")
    add_deps("freestanding", "debug")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "attacker",
                priority = 1,
                entry_point = "run",
                stack_size = 0x1000,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)

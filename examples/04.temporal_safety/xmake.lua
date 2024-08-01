-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT temporal safety")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib"))

option("board")
    set_default("sail")

compartment("allocate")
    -- memcpy
    add_deps("freestanding", "debug")
    add_files("allocate.cc")

compartment("claimant")
    add_deps("debug")
    add_files("claimant.cc")

-- Firmware image for the example.
firmware("temporal_safety")
    add_deps("allocate")
    add_deps("claimant")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "allocate",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x400,
                trusted_stack_frames = 4
            }
        }, {expand = false})
    end)

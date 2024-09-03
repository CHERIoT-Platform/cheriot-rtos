-- Copyright Google LLC and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT bogomips estimator")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

option("board")
    set_default("sail")

-- Support libraries
includes(path.join(sdkdir, "lib"))

-- bogomips application.
compartment("bogomips")
    add_files("bogomips.cc")
    add_defines("CHERIOT_NO_AMBIENT_MALLOC")

-- Firmware image.
firmware("bogomips-firmware")
    add_deps("bogomips")
    add_deps("freestanding", "debug")
    on_load(function(target)
        target:values_set("board", "$(board)")
        -- NB: trusted_stack_frames is a guess; +1'd for any
        --     compartment error handler usage?
        local threads = {
            {
                compartment = "bogomips",
                priority = 1,
                entry_point = "entry",
                stack_size = 0x1000, -- 4KB
                trusted_stack_frames = 5
            },
        }
        target:values_set("threads", threads, {expand = false})
    end)

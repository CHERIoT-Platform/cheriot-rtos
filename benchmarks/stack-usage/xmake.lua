-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT stack-usage benchmark");
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/crt"))

option("board")
    set_default("sail")

compartment("callee")
    add_files("callee.cc")

compartment("caller")
    add_defines("BOARD=" .. tostring(get_config("board")))
    add_files("caller.cc")

-- Firmware image for the example.
firmware("stack-usage-benchmark")
    add_deps("crt", "freestanding", "atomic")
    add_deps("caller", "callee")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "caller",
                priority = 1,
                entry_point = "run",
                stack_size = 0x1000,
                trusted_stack_frames = 3
            }
        }, {expand = false})
    end)

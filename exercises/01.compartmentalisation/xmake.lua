-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Hello World")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/string"),
         path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/microvium"),
         path.join(sdkdir, "lib/crt"))

option("board")
    set_default("ibex-safe-simulator")

compartment("entry")
    add_files("entry.cc")

compartment("js")
    add_files("js.cc")

compartment("secret")
    add_files("secret.cc")

-- Firmware image for the example.
firmware("javascript")
    add_deps("crt", "freestanding", "string", "microvium", "atomic_fixed")
    add_deps("js")
    add_deps("entry")
    add_deps("secret")
    add_deps("debug")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "entry",
                priority = 1,
                entry_point = "run",
                stack_size = 0x800,
                trusted_stack_frames = 5
            }
        }, {expand = false})
    end)

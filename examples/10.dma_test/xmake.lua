-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Simple DMA App")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/crt"))

option("board")
    set_default("sail")

compartment("dma")
    add_files(path.join(sdkdir, "core/dma/dma_compartment.cc"))

compartment("dma_app")
    add_files("dma_test.cc")

-- Firmware image for the example.
firmware("dma_test")
    add_deps("crt", "freestanding", "atomic_fixed")
    add_deps("dma", "dma_app")
    -- add_deps("dma_app")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "dma_app",
                priority = 1,
                entry_point = "dma_request",
                stack_size = 0x400,
                trusted_stack_frames = 2
            }
        }, {expand = false})
    end)

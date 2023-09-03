-- Copyright Microsoft and CHERIoT Contributors.
-- SPDX-License-Identifier: MIT

set_project("CHERIoT Simple DMA App")
sdkdir = "../../sdk"
includes(sdkdir)
set_toolchains("cheriot-clang")

-- Support libraries
includes(path.join(sdkdir, "lib/freestanding"),
         path.join(sdkdir, "lib/cxxrt"),
         path.join(sdkdir, "lib/atomic"),
         path.join(sdkdir, "lib/crt"))

option("board")
    set_default("sail")

compartment("dma")
    add_files(path.join(sdkdir, "core/dma-v2/dma_compartment.cc"))

compartment("dma_test")
    add_files("dma-test-v2.cc")

-- Firmware image for the example.
firmware("dma-test-v2")
    add_deps("crt", "cxxrt", "freestanding", "atomic_fixed")
    add_deps("dma", "dma_test")
    on_load(function(target)
        target:values_set("board", "$(board)")
        target:values_set("threads", {
            {
                compartment = "dma_test",
                priority = 2,
                entry_point = "test_dma",
                stack_size = 0x1000,
                trusted_stack_frames = 9
            }
        }, {expand = false})
    end)

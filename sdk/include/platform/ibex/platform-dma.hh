#pragma once

#define MALLOC_QUOTA 0x100000
#define TEST_NAME "Allocator"
#include "tests.hh"

#include <cheri.hh>
#include <compartment-macros.h>
#include <cstdint>
#include <stdint.h>
#include <utils.hh>

#include <thread.h>
#include <thread_pool.h>

using thread_pool::async;
DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(dmaDriverHeap, 8144);

namespace Ibex {
    class GenericDMA {
        private:

        Timeout noWait{0};
        SObjStruct *dmaHeap;

        struct StreamingInterface
        {
            uint32_t control;
            uint32_t status;
            uint32_t address;
            uint32_t lengthInBytes;
        };

        struct DMAInterface
        {
            /**
             *  these two streaming interfaces are for 
             *  [0] memory to target and 
             *  [1] target to memory directions.
             *  target here is any I/O device, peripheral or an accelerator
             */
            volatile StreamingInterface *streamingInterface[2];
        };

        volatile DMAInterface *dmaInterface;

        public:

        void init()
        {
            dmaInterface->streamingInterface[0] = MMIO_CAPABILITY(StreamingInterface, dma_to_target);
            dmaInterface->streamingInterface[1] = MMIO_CAPABILITY(StreamingInterface, dma_to_memory);
        }

        uint32_t read_status(bool index)
        {   
            /**
             *  this statement returns the less signifance bit 
             *  to show the halted status
             */
            return dmaInterface->streamingInterface[index]->status & 0x1;
        }

        void write_conf(bool index, uint32_t *address, uint32_t lengthInBytes)
        {
            /**
             *  filling address and length fields
             */
            dmaInterface->streamingInterface[index]->address = *address;
            dmaInterface->streamingInterface[index]->lengthInBytes = lengthInBytes;

            /**
             *  create a heap capability for dma below. 
             *  todo: we assume that this heap capability can serve 
             *  for different compartments, irrespective of 
             *  the origin because this is a driver code?
             */
            dmaHeap = STATIC_SEALED_VALUE(dmaDriverHeap);

            /**
             *  claim the allocated memory 
             */

            int  claimCount = 0;

            auto claim      = [&]() {
                size_t claimSize = heap_claim(dmaHeap, address);
                claimCount++;
                TEST(claimSize == lengthInBytes,
                        "{}-byte allocation claimed as {} bytes (claim number {})",
                        lengthInBytes,
                        claimSize,
                        claimCount);
            };

            claim();
        }

        void start_dma(bool index)
        {
            /**
             *  setting a start bit
             */
            dmaInterface->streamingInterface[index]->control = 0x1;
        }

        void reset_dma(bool index, uint32_t *address)
        {
            /**
             *  setting a reset bit
             */
            dmaInterface->streamingInterface[index]->control = 0x4;

            int ret = heap_free(dmaHeap, address);
        }

    };
} // namespace Ibex
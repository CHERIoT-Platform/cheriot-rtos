#pragma once

#include <compartment-macros.h>
#include <cstdio>
#include <utils.hh>

namespace Ibex {
    class PlatformDMA {
        private:

        struct DMAInterface
        {
            /**
             * Below is DMA control register address: 
             *  - includes start('b0), endianness conversion('b1 and 2) and reset bits('b3)
             *  - bit of index 1 and 2 are for enabling 2 and 4 byte swaps respectively
             *  - start bit is meant to be set at the end while programming
             */
            uint32_t control;
            /**
             * Below is DMA status register address 
             *  - first bit refers to halted status
             *  - 0 is for idle, and 1 for running
             */
            uint32_t status;
            /**
             * Below is DMA source address register address: 
             *  - here source address is where FROM the DMA should transfer the data
             *  - it can be either memory buffer or MMAP-ed I/O device (i.e. any peripheral or accelerator)
             */
            uint32_t sourceAddress;
            /**
             * Below is DMA target address register address: 
             *  - here target address is where TO the DMA should transfer the data
             *  - it can be either memory buffer or MMAP-ed I/O device (i.e. any peripheral or accelerator)
             */
            uint32_t targetAddress;
            /**
             * Below is the amount of data IN BYTES that DMA should transfer 
             * between the source and a target 
             */
            uint32_t lengthInBytes;
            /**
             * Below is the amount of strides IN 4 BYTE WORDS for a source address. 
             * Strides is the jump amount between each data retrieval address
             * during the DMA operation.
             * Minimum width of the stride is 4 byte words.
             *  - 0 stride is default and points to the address of the next word 
             *  - 1 is for 1 word skippal, and 2 for 2 before the next address
             *  - todo: more fine grain stride configurability (in term of bytes) for the future?
             */
            uint32_t sourceStrides;
            /**
             * Below is the amount of strides IN 4 BYTE WORDS for a target address. 
             * The same as above but just for a target address. So that we can fetch 
            *  and transfer the data at different stride rates
             */
            uint32_t targetStrides;
        };

        volatile DMAInterface *dmaInterface;

        public:

        void init()
        {
            dmaInterface = MMIO_CAPABILITY(DMAInterface, dma);
        }

        uint32_t read_status()
        {   
            /**
             *  this statement returns the less signifance bit 
             *  to show the halted status
             */
            return dmaInterface->status & 0x1;
        }

        void write_conf(uint32_t sourceAddress, uint32_t targetAddress, uint32_t lengthInBytes)
        {
            /**
             *  filling source and target addresses, and length fields
             */
            dmaInterface->sourceAddress = sourceAddress;
            dmaInterface->targetAddress = targetAddress;
            dmaInterface->lengthInBytes = lengthInBytes;

        }

        void write_strides(uint32_t sourceStrides, uint32_t targetStrides)
        {
            /**
             *  filling source and target strides
             */
            dmaInterface->sourceStrides = sourceStrides;
            dmaInterface->targetStrides = targetStrides;
        }

        int byte_swap_en(uint32_t swapAmount)
        {
            /**
             *  filling byte swaps
             */

            if (swapAmount == 2) 
            {
                dmaInterface->control = 0x2;
            } else if (swapAmount == 4)
            {
                dmaInterface->control = 0x4;
            } else 
            {
                return -1;
            }

            return 0;
        }

        void start_dma()
        {
            /**
             *  setting a start bit
             */
            dmaInterface->control = 0x1;
        }

        void reset_dma()
        {
            /**
             *  setting a reset bit, which is bit 3
             */
            dmaInterface->control = 0x8;

        }

    };
} // namespace Ibex

template<typename WordT, size_t TCMBaseAddr>
using PlatformDMA = Ibex::PlatformDMA;
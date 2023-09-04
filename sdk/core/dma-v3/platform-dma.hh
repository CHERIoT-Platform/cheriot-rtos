#pragma once

#include <cheri.hh>
#include <compartment-macros.h>
#include <cstdint>
#include <cstdio>
#include <debug.hh>
#include <utils.hh>

namespace Ibex
{
	class PlatformDMA
	{
		private:
		struct DMAInterface
		{
			/**
			 * Below is DMA source capability register address:
			 *  - here source address is where FROM the DMA should transfer the
			 * data
			 *  - it can be either memory buffer or MMAP-ed I/O device (i.e. any
			 * peripheral or accelerator)
			 *  - this capability also includes the both address and the access bits
			 *  - regs 0 and 1 at hw
			 */
			void* sourceCapability;
			/**
			 * Below is DMA target capability register address:
			 *  - here target address is where TO the DMA should transfer the
			 * data
			 *  - it can be either memory buffer or MMAP-ed I/O device (i.e. any
			 * peripheral or accelerator)
			 *  - this capability also includes the both address and the access bits
			 *  - regs 2 and 3 at hw
			 */
			void* targetCapability;
			/**
			 * Below is the amount of data IN BYTES that DMA should transfer
			 * between the source and a target
			 *  - regs 4 at hw
			 */
			uint32_t lengthInBytes;
			/**
			 * Below is the amount of strides IN 4 BYTE WORDS for a source
			 * address. Strides is the jump amount between each data retrieval
			 * address during the DMA operation. Minimum width of the stride is
			 * 4 byte words.
			 *  - 0 stride is default and points to the address of the next word
			 *  - 1 is for 1 word skippal, and 2 for 2 before the next address
			 *  - todo: more fine grain stride configurability (in term of
			 * bytes) for the future?
			 *  - regs 5 at hw
			 */
			uint32_t sourceStrides;
			/**
			 * Below is the amount of strides IN 4 BYTE WORDS for a target
			 * address. The same as above but just for a target address. So that
			 * we can fetch and transfer the data at different stride rates
			 *  - regs 6 at hw
			 */
			uint32_t targetStrides;
			/**
			 * Below is DMA control register address:
			 *  - includes start('b0), endianness conversion('b1 and 2) and
			 * reset bits('b3)
			 *  - bit of index 1 and 2 are for enabling 2 and 4 byte swaps
			 * respectively
			 *  - start bit is meant to be set at the end while programming
			 *  - regs 7 at hw
			 */
			uint32_t control;
			/**
			 * Below is DMA status register address
			 *  - first bit refers to halted status
			 *  - 0 is for idle, and 1 for running
			 *  - regs 8 at hw
			 */
			uint32_t status;
			/**
			 * Below is the MMIO interface to tell the DMA that free() 
			 * call occurred at the allocator compartment 
			 *  - regs 9 at hw
			 */
			uint32_t callFromMalloc;
		};

		__always_inline volatile DMAInterface &device()
		{
			return *MMIO_CAPABILITY(DMAInterface, dma);
		}

		void write_strides(uint32_t sourceStrides, uint32_t targetStrides)
		{
			/**
			 *  Setting source and target strides
			 */
			device().sourceStrides = sourceStrides;
			device().targetStrides = targetStrides;
		}

		int swap_bytes_and_start_dma(uint32_t swapAmount)
		{
			/**
			 *  Setting byte swaps
			 *  and start bit.
			 *
			 *  Swap amount can be equal to
			 *  only either 2 or 4.
			 */

			if (swapAmount != 2)
			{
				if (swapAmount != 4)
				{
					swapAmount = 0;
				}
			}

			uint32_t controlConfiguration = swapAmount | 0x1;

			device().control = controlConfiguration;

			return 0;
		}

		public:
		uint32_t read_status()
		{
			/**
			 *  this statement returns the less signifance bit
			 *  to show the halted status
			 */
			return device().status & 0x1;
		}

		void write_conf_and_start(uint32_t *sourceAddress,
		                          uint32_t *targetAddress,
		                          uint32_t  lengthInBytes,
		                          uint32_t  sourceStrides,
		                          uint32_t  targetStrides,
		                          uint32_t  byteSwapAmount)
		{
			/**
			 *  Setting source and target addresses, and length fields
			 */
			device().sourceCapability = (void*) sourceAddress;
			device().targetCapability = (void*) targetAddress;
			device().lengthInBytes = lengthInBytes;

			write_strides(sourceStrides, targetStrides);

			swap_bytes_and_start_dma(byteSwapAmount);
		}
		
		void notify_the_dma()
		{
			device().callFromMalloc = 1;
		}

		void reset_dma()
		{
			/**
			 *  Setting a reset bit, which is bit 3.
			 *  this clears all the registers,
			 *  but do not transfer the current transfer status anywhere
			 */
			device().control = 0x8;
		}
	};
} // namespace Ibex

// template<typename WordT, size_t TCMBaseAddr>
using PlatformDMA = Ibex::PlatformDMA;
#pragma once

#include <cheri.hh>
#include <compartment-macros.h>
#include <cstdint>
#include <cstdio>
#include <debug.hh>
#include <utils.hh>

namespace Ibex
{
	/**
		 *  Drivers should obey this DMADescriptor structure
		 *
		 *  If they do not obey the right structure and the addresses, 
		 *  bounds and permissions are wrong, DMA does not start.
		 */
		struct DMADescriptor
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
			 *  Below is the register to show the number of byte swaps to be 2 or 4.
			 *  This will be checked for the right value at the hw again.
			 *  - regs 7 at hw
			 */
			uint32_t byteSwaps;
			/**
			 * Ideally the following two registers should be set only 
			 *  at the compartment!
			 */
			 
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
			 *  - first bit refers to success status
			 *  - regs 8 at hw
			 */
			uint32_t status;
		};

	class PlatformDMA
	{
		private:

		struct DMAInterface
		{
			/**
			 * All configurations would be written to the hw via 
			 * setting the dmaDescriptorPointer to the right descriptor
			 * 
			 * The structure of the descriptor is written here.
			 * This driver does not use tne reset register and resets 
			 * automatically.
			 */

			DMADescriptor *dmaDescriptorPointer;
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

		public:

		void write_conf_and_start(DMADescriptor *dmaDescriptorPointer)
		{
			/**
			 *  Setting configurations via pointing 
			 *  to a descriptor adddress.
			 *
			 *  DMA controller should be able to fetch the data on its own
			 */
			device().dmaDescriptorPointer = dmaDescriptorPointer;
		}
		
		void notify_the_dma()
		{
			device().callFromMalloc = 1;
		}

		void reset_dma(DMADescriptor *updatedDescriptorPointer)
		{
			/**
			 *  Setting a reset bit, which is bit 3.
			 *  this clears all the registers,
			 *  but do not transfer the current transfer status anywhere.
			 *  
			 *  DMA driver should send the pointer with updated control 
			 *  field to enable this change.
			 * 
			 *  HW will check whether initiator and reset descriptor pointer
			 *  are similar to guarantee the correctness
			 */
			device().dmaDescriptorPointer = updatedDescriptorPointer;
		}
	};
} // namespace Ibex

// template<typename WordT, size_t TCMBaseAddr>
using PlatformDMA = Ibex::PlatformDMA;
#include "dma_compartment.hh"
#include "futex.h"
#include "platform-dma.hh"
#include <concepts>
#include <cstddef>
#include <errno.h>
#include <interrupt.h>
#include <stdint.h>
#include <utils.hh>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "DMA Compartment">;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(dmaInterruptCapability,
                                        dma,
                                        true,
                                        true);

using namespace Ibex;

namespace DMA
{

	template<typename T>  
	concept IsDmaDevice = requires(T         	  device,
	                               DMADescriptor *dmaDescriptorPointer)
	{
		{
			device.write_conf_and_start(dmaDescriptorPointer)
			} -> std::same_as<void>;
		{
			device.reset_dma(dmaDescriptorPointer)
			} -> std::same_as<void>;
	};

	template<typename PlatformDMA>

	requires IsDmaDevice<PlatformDMA>

	class GenericDMA : public PlatformDMA
	{
		public:
		int configure_and_launch(void *sourceAddress,
		                         void *targetAddress,
		                         uint32_t  lengthInBytes,
		                         uint32_t  sourceStrides,
		                         uint32_t  targetStrides,
		                         uint32_t  byteSwapAmount)
		{
			/**
			 *  Dma launch call:
			 *  - checks for the dma ownership status,
			 *  - for access rights,
			 *  - creates claims for each source and destination addresses,
			 *  - automatically resets the claims and the dma registers
			 *    at the end of the transfer.
			 */
			Debug::log("before launch");
			
			DMADescriptor *dmaDescriptorPointer;

			/**
			 *  Set the configurations here,
			 *  before sending the descriptor to the DMA
			 */
			dmaDescriptorPointer->sourceCapability = sourceAddress;
			dmaDescriptorPointer->targetCapability = targetAddress;
			dmaDescriptorPointer->lengthInBytes    = lengthInBytes;
			dmaDescriptorPointer->sourceStrides    = sourceStrides;
			dmaDescriptorPointer->targetStrides    = targetStrides;
			dmaDescriptorPointer->byteSwaps		   = byteSwapAmount;

			int dmaInterruptReturn = launch_dma(dmaDescriptorPointer);

			Debug::log("after launch: {}", dmaInterruptReturn);

			int freeStatus;
			freeStatus = free(sourceAddress);
			Debug::log("driver, freeStatus: {}", freeStatus);

			if (dmaInterruptReturn < 0) 
			{
				return -EINVAL;
			}

			// todo: implement a do-while loop later in case!
			int restartReturn = wait_and_reset_dma(dmaInterruptReturn, dmaDescriptorPointer);

			Debug::log("after restart: {}", restartReturn);

			return 0;
		}
	};

	using Device = GenericDMA<PlatformDMA>;
} // namespace DMA

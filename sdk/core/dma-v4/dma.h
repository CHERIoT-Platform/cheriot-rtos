#include "futex.h"
#include "platform-dma.hh"
#include <concepts>
#include <cstddef>
#include <errno.h>
#include <interrupt.h>
#include <stdint.h>
#include <utils.hh>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "DMA Driver">;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(dmaInterruptCapability,
                                        dma,
                                        true,
                                        true);

using namespace Ibex;

Ibex::PlatformDMA platformDma;

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
		int configure_and_launch(DMADescriptor *dmaDescriptorPointer)
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

			platformDma.write_conf_and_start(dmaDescriptorPointer);

			static const uint32_t *dmaFutex =
	  		interrupt_futex_get(STATIC_SEALED_VALUE(dmaInterruptCapability));

			uint32_t currentInterruptCounter = *dmaFutex;

			Debug::log("after launch: {}", currentInterruptCounter);
			
			// int freeStatus;
			// freeStatus = free(sourceAddress);
			// Debug::log("driver, freeStatus: {}", freeStatus);

			// todo: implement a do-while loop later in case!

			Timeout t{10};
			futex_timed_wait(&t, dmaFutex, currentInterruptCounter);

			int startedStatus = dmaDescriptorPointer->status;

			if (startedStatus == 1)
			{
				/**
				*  Resetting the dma registers
				*  todo: we need some check here to avoid double checks
				*/
				Debug::log("inside the reset condition");

				platformDma.reset_dma(dmaDescriptorPointer);

				/**
				*  Acknowledging interrupt here irrespective of the reset status
				*/
				interrupt_complete(STATIC_SEALED_VALUE(dmaInterruptCapability));

				return 0;
			}

			Debug::log("after restart");

			return 0;
		}
	};

	using Device = GenericDMA<PlatformDMA>;
} // namespace DMA

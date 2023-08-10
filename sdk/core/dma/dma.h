#include "dma_compartment.hh"
#include "futex.h"
#include "platform-dma.hh"
#include <concepts>
#include <cstddef>
#include <stdint.h>
#include <utils.hh>
#include <interrupt.h>
#include <errno.h>

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(dmaInterruptCapability, dma, true, true);

namespace DMA 
{		

    template<typename T>
	concept IsDmaDevice = requires(T device, uint32_t *sourceAddress, uint32_t *targetAddress, size_t lengthInBytes,
                                        uint32_t sourceStrides, uint32_t targetStrides, uint32_t byteSwapAmount)
	{
		{
			device.write_conf_and_start(sourceAddress, targetAddress, lengthInBytes,
											sourceStrides, targetStrides, byteSwapAmount)
			} -> std::same_as<void>;
        {
			device.reset_dma()
			} -> std::same_as<void>;
	};

    template<typename PlatformDMA>

	requires IsDmaDevice<PlatformDMA>

	class GenericDMA : public PlatformDMA
	{
		public:

        int configure_and_launch(uint32_t *sourceAddress, uint32_t *targetAddress, uint32_t lengthInBytes,
                        uint32_t sourceStrides, uint32_t targetStrides, uint32_t byteSwapAmount)
        {	
			/**
			 *  Fetch the interrupt counter for the dma
			 *  to use for future operations
			 */

			const uint32_t *dmaFutex = interrupt_futex_get(STATIC_SEALED_VALUE(dmaInterruptCapability));

			/** 
			 *  Dma launch call: 
			 *  - checks for the dma ownership status, 
			 *  - for access rights,
			 *  - creates claims for each source and destination addresses,
			 *  - automatically resets the claims and the dma registers
			 *    at the end of the transfer.
			 */
			
			int dmaInterruptReturn = launch_dma(sourceAddress, targetAddress, lengthInBytes,
                        sourceStrides, targetStrides, byteSwapAmount);
			
			/**
			 *  Negative interrupt return is for 
			 *  the failed dma launch and forces to wait for the interrupt.
			 *  However, wait only until timeout elapses
			 */

			Timeout t{10};

			while (dmaInterruptReturn < 0)
			{	
				uint32_t realInterruptNumber = -(dmaInterruptReturn)-1;

				futex_timed_wait(&t, dmaFutex, realInterruptNumber);

				dmaInterruptReturn = launch_dma(sourceAddress, targetAddress, lengthInBytes,
                        sourceStrides, targetStrides, byteSwapAmount);

				if (!t.remaining)
				{
					return -EINVAL;
				}
			}

			/**
			 *  Handle the interrupt here, once dmaFutex woke up via scheduler.
			 *  DMA interrupt means that the dma operation is finished 
			 *  and it is time to reset and clear the dma configuration registers.
			 *  Unlike with futex wait of other threads, as an occupying thread we 
			 *  wait indefinitely as much as needed for the dma completion			
			 */
			futex_wait(dmaFutex, dmaInterruptReturn);

    		reset_and_clear_dma(dmaInterruptReturn);

			return 0;
		}

	};

    using Device = GenericDMA<PlatformDMA>;
}


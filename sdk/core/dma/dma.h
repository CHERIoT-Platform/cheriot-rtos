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
            int launchDmaStatus = launch_dma(sourceAddress, targetAddress, lengthInBytes,
                        sourceStrides, targetStrides, byteSwapAmount);

			
			// once dma is launched, we need to check for the interrupt status
			// no need for validity and permissions checks for the scheduler once futex is created as well
			const uint32_t *dmaFutex = interrupt_futex_get(STATIC_SEALED_VALUE(dmaInterruptCapability));

			uint32_t previous = *dmaFutex;
			Debug::log("intc futex val: {}", previous);

			// sleep until interrupt fires with futex_wait
			futex_wait(dmaFutex, previous);

			// Handle the interrupt here
			// dma interrupt means that the dma operation is finished 
			// and it is time to reset and clear the dma configurations
			
			// non 0 return means fail
			int resetFailed = reset_and_clear_dma(previous);

			// Acknowledging interrupt here
			interrupt_complete(STATIC_SEALED_VALUE(dmaInterruptCapability));
			// todo: once interrupt is served we just return?

			if (resetFailed)
			{
				return -EINVAL;
			} 


			return 0;
			
		}

	};

    using Device = GenericDMA<PlatformDMA>;
}


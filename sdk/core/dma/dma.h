#include "dma_compartment.h"
#include "platform-dma.hh"
#include <concepts>
#include <cstddef>
#include <stdint.h>
#include <utils.hh>

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
            return dma_compartment(sourceAddress, targetAddress, lengthInBytes,
                        sourceStrides, targetStrides, byteSwapAmount);
        }

	};

    using Device = GenericDMA<PlatformDMA>;
}


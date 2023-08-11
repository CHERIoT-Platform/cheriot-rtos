#include "dma_compartment.hh"
#include "futex.h"
#include "platform-dma.hh"
#include <concepts>
#include <cstddef>
#include <errno.h>
#include <interrupt.h>
#include <stdint.h>
#include <utils.hh>


DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(dmaInterruptCapability,
                                        dma,
                                        true,
                                        true);

namespace DMA
{

	template<typename T>
	concept IsDmaDevice = requires(T         device,
	                               uint32_t *sourceAddress,
	                               uint32_t *targetAddress,
	                               size_t    lengthInBytes,
	                               uint32_t  sourceStrides,
	                               uint32_t  targetStrides,
	                               uint32_t  byteSwapAmount)
	{
		{
			device.write_conf_and_start(sourceAddress,
			                            targetAddress,
			                            lengthInBytes,
			                            sourceStrides,
			                            targetStrides,
			                            byteSwapAmount)
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
		int configure_and_launch(uint32_t *sourceAddress,
		                         uint32_t *targetAddress,
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

			int dmaInterruptReturn = launch_dma(sourceAddress,
			                                    targetAddress,
			                                    lengthInBytes,
			                                    sourceStrides,
			                                    targetStrides,
			                                    byteSwapAmount);

			return 0;
		}
	};

	using Device = GenericDMA<PlatformDMA>;
} // namespace DMA

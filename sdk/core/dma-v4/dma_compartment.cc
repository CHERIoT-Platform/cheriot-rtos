#include "futex.h"

#include "dma_compartment.hh"
#include <cstdint>
#include <debug.hh>
#include <memory>

#include "platform-dma.hh"
#include <cheri.hh>
#include <compartment-macros.h>
#include <errno.h>
#include <interrupt.h>
#include <locks.hh>
#include <utils.hh>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "DMA Compartment">;

// Import some useful things from the CHERI namespace.
using namespace CHERI;
using namespace Ibex;

Ibex::PlatformDMA platformDma;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(dmaInterruptCapability,
                                        dma,
                                        true,
                                        true);

int launch_dma(DMADescriptor *dmaDescriptorPointer)
{
	/**
	 *  No lock necessary due to single write point
	 *  and automatic reset
	 */

	/**
	 *  No checks at the driver at this version.
	 *
	 *  So, once returned after interrupt and reset, if any,
	 *  we write to the DMA and start the transfer
	 */

	/**
	 *  Control register are set here for the start bit.
	 *  Even it is set before at the drive, it is reset here
	 */
	dmaDescriptorPointer->control = 1;
	platformDma.write_conf_and_start(dmaDescriptorPointer);

	static const uint32_t *dmaFutex =
	  interrupt_futex_get(STATIC_SEALED_VALUE(dmaInterruptCapability));

	uint32_t currentInterruptCounter = *dmaFutex;

	/**
	 *  return the interrupt,
	 *  so that initiator can wait for it
	 */

	return currentInterruptCounter;
}

int wait_and_reset_dma(uint32_t       interruptNumber,
                        DMADescriptor *originalDescriptorPointer)
{
	/**
	 *  Handle the interrupt here, once dmaFutex woke up via scheduler.
	 *  DMA interrupt means that the dma operation is finished
	 *  and it is time to reset and clear the dma configuration registers.
	 */

	/**
	 *  However, clear only if the addresses are not reset yet.
	 *  Because this function can be called from two different points
	 */

	static const uint32_t *dmaFutex =
	  interrupt_futex_get(STATIC_SEALED_VALUE(dmaInterruptCapability));

	Debug::log("before futex wait");

	Timeout t{10};
	futex_timed_wait(&t, dmaFutex, interruptNumber);

	int startedStatus = originalDescriptorPointer->status;

	if (startedStatus == 1)
	{
		/**
		 *  Resetting the dma registers
		 *  todo: we need some check here to avoid double checks
		 */
		Debug::log("inside the reset condition");

		originalDescriptorPointer->control = 8;

		platformDma.reset_dma(originalDescriptorPointer);

		/**
		 *  Acknowledging interrupt here irrespective of the reset status
		 */
		interrupt_complete(STATIC_SEALED_VALUE(dmaInterruptCapability));

		return 0;
	}

	return 1;
}

void force_stop_dma(DMADescriptor *originalDescriptorPointer)
{
	/**
	 *  Calling the platform function to forcefully reset the DMA
	 *  with updated ptr.
	 *  
	 *  However, DMA controller will still conduct checks to make sure
	 *  that the right descriptor only can abort the transfer 
	 */
	 
	originalDescriptorPointer->control = 8;
	platformDma.reset_dma(originalDescriptorPointer);
}
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

Ibex::PlatformDMA platformDma;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(dmaInterruptCapability,
                                        dma,
                                        true,
                                        true);

namespace
{
	/**
	 * Flag lock to control ownership
	 * over the dma controller
	 */
	FlagLock dmaOwnershipLock;

	uint32_t expectedValue = 0;

	bool alreadyReset = 0;


} // namespace

void internal_wait_and_reset_dma(uint32_t interruptNumber);

int launch_dma(uint32_t *sourceAddress,
               uint32_t *targetAddress,
               uint32_t  lengthInBytes,
               uint32_t  sourceStrides,
               uint32_t  targetStrides,
               uint32_t  byteSwapAmount)
{
	/**
	 *  Lock this compartment via LockGuard,
	 *  to prevent data race.
	 *
	 *  This lock automatically unlocks at the end of this function.
	 */
	LockGuard g{dmaOwnershipLock};

	/**
	 *  If dma is already launched, we need to check for the interrupt status.
	 *  No need for validity and permissions checks though for the scheduler
	 *  once futex is created
	 */

	static const uint32_t *dmaFutex =
	  interrupt_futex_get(STATIC_SEALED_VALUE(dmaInterruptCapability));

	uint32_t currentInterruptCounter = *dmaFutex;

	/**
	 *  If dma is already running, check for the
	 *  expected and current interrupt values.
	 *  If they do not match, wait for the interrupt.
	 *
	 *  Expected Value is expected to be incremented only per thread,
	 *  assuming that every thread enters the launch_dma()
	 *  only once per each transfer
	 */

	if (expectedValue != currentInterruptCounter)
	{
		internal_wait_and_reset_dma(currentInterruptCounter);
	}

	Debug::Assert(
	  expectedValue == *dmaFutex,
	  "ExpectedValue is not equal to the current interrupt counter!");

	/**
	 *  No checks at the driver at this version.
	 *
	 *  So, once returned after interrupt and reset, if any,
	 *  we write to the DMA and start the transfer 
	 */

	alreadyReset = 0;

	platformDma.write_conf_and_start(sourceAddress,
	                                 targetAddress,
	                                 lengthInBytes,
	                                 sourceStrides,
	                                 targetStrides,
	                                 byteSwapAmount);

	/**
	 *  Increment the expected value only when
	 *  dma has started to avoid the potential deadlock
	 *  of this function is returned with failure earlier
	 */
	expectedValue++;

	/**
	 *  return here, if all operations
	 *  were successful.
	 */

	return currentInterruptCounter;
}

void internal_wait_and_reset_dma(uint32_t interruptNumber)
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

	if (!alreadyReset) 
	{
		/**
	 	 *  Resetting the dma registers
	 	 *  todo: we need some check here to avoid double checks
	 	 */
		Debug::log("inside the reset condition");

	 	platformDma.reset_dma();

		alreadyReset = 1;

		/**
		 *  Acknowledging interrupt here irrespective of the reset status
		 */
		interrupt_complete(STATIC_SEALED_VALUE(dmaInterruptCapability));
	} 
	
}

void wait_and_reset_dma(uint32_t interruptNumber)
{	
	/**
	 *  This lock is to avoid the data race as well	
	 */
	LockGuard g{dmaOwnershipLock};

	internal_wait_and_reset_dma(interruptNumber);	
}
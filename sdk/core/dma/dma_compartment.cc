#include "futex.h"
#define MALLOC_QUOTA 0x100000

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

	uint32_t dmaIsLaunched = 0;
	uint32_t expectedValue = 0;

	/**
	 * Claims pointers so that the following
	 * addresses will not be freed ahead of time
	 */
	std::unique_ptr<char> claimedSource;
	std::unique_ptr<char> claimedDestination;

} // namespace

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

	const uint32_t *dmaFutex =
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
		Timeout t{10};
		futex_timed_wait(&t, dmaFutex, expectedValue - 1);

		reset_and_clear_dma();
	}

	/**
	 *  After acquiring a ownership over lock,
	 *  now it is the time to launch dma.
	 *
	 *  Here, we claim the memory with default malloc capability,
	 *  as declaring another heap capability is an extra entry
	 *  that leaves the default capability let unused.
	 *
	 *  Unique pointers are used to avoid explicit heap_free() calls.
	 *  when this pointer is not used anymore, it is automatically deleted
	 */

	auto claim = [](void *ptr) -> std::unique_ptr<char> {
		if (heap_claim(MALLOC_CAPABILITY, ptr) == 0)
		{
			return {nullptr};
		}

		return std::unique_ptr<char>{static_cast<char *>(ptr)};
	};

	claimedSource      = claim(sourceAddress);
	claimedDestination = claim(targetAddress);

	if (!claimedSource || !claimedDestination)
	{
		return -EINVAL;
	}

	/**
	 *  return if sufficient permissions are not present
	 *  and if not long enough
	 */

	if (!check_pointer<PermissionSet{Permission::Load, Permission::Global}>(
	      sourceAddress, lengthInBytes) ||
	    !check_pointer<PermissionSet{Permission::Store, Permission::Global}>(
	      targetAddress, lengthInBytes))
	{
		return -EINVAL;
	}

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
	 *  Handle the interrupt here, once dmaFutex woke up via scheduler.
	 *  DMA interrupt means that the dma operation is finished
	 *  and it is time to reset and clear the dma configuration registers.
	 *  Unlike with futex wait of other threads, as an occupying thread we
	 *  wait indefinitely as much as needed for the dma completion
	 */
	futex_wait(dmaFutex, currentInterruptCounter);

	reset_and_clear_dma();

	/**
	 *  return here, if all operations
	 *  were successful.
	 */

	return 0;
}

void reset_and_clear_dma()
{
	/**
	 *  Resetting the claim pointers
	 *  and cleaning up the dma registers.
	 *
	 *  Claim registers are meant to be
	 *  cleared by every DMA operation,
	 *  that is why we are explicitely resetting
	 *  them at this exit function.
	 */

	/**
	 *  However, clear only if the addresses are not reset yet.
	 *  Because this function can be called from two different points
	 */

	LockGuard g{dmaOwnershipLock};

	if (claimedSource || claimedDestination)
	{
		Debug::log("before dropping claims");
		claimedSource.reset();
		claimedDestination.reset();

		/**
		 *  Resetting the dma registers
		 */
		platformDma.reset_dma();
		
		/**
		 *  Acknowledging interrupt here irrespective of the reset status
		 */
		interrupt_complete(STATIC_SEALED_VALUE(dmaInterruptCapability));
	}
}

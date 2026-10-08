// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
/**
 * \file
 *
 * This file describes the interfaces for compartments to wait for interrupts.
 * Interrupts are exposed to compartments as futexes that contain the count of
 * the number of times that an interrupt has fired.  This count can wrap but
 * that should not be visible in practice (if will be observable only if one
 * thread handles a specific interrupt 2^32 times in between another thread
 * finishing handling that interrupt and waiting again).
 *
 * The flow for waiting for an interrupt involves the following steps:
 *
 *  1. Request the futex word for a particular from the scheduler.
 *  2. Wait on the futex.
 *  3. Read the value of the futex word.
 *  4. Handle whatever the interrupt was raised for.
 *  5. Mark the interrupt as completed.
 *  6. Loop from step 2, using the value read from step 3.
 *
 * If the interrupt fires between steps 5 and 6 then the futex word will not
 * match the value read in step 3 and the futex wait will return immediately.
 *
 * The first time that step 2 is reached, the expected value for the futex
 * should be 0.  This ensures that you acknowledge any interrupt that happened
 * in between the system starting and your registering interest in the
 * interrupt.
 *
 * Note that step 2 may use a multiwaiter, rather than a single futex_wait
 * call, if you wish to wait for one of multiple event sources.
 *
 * Both step 1 and 5 require an authorising capability, as described below.
 */

#include <compartment.h>
#include <stdbool.h>

#ifndef CLANG_TIDY
#	if !__has_include("board-interrupts.h")
#		error Including <interrupt.h> w/o cheriot.board.interrupts dependency
#	endif
#	include "board-interrupts.h"
#else

enum InterruptName : uint16_t
{
	FakeInterrupt            = 4,
	RevokerInterrupt         = 5,
	EthernetReceiveInterrupt = 3,
	EthernetInterrupt        = 47
};

#endif

/**
 * Structure for authorising access to a specific interrupt.
 */
struct InterruptCapabilityState
{
	/**
	 * The interrupt number that this refers to.
	 */
	enum InterruptName interruptNumber;
	/**
	 * Does this authorise accessing the futex for monitoring the interrupt?
	 */
	bool mayWait;
	/**
	 * Does this authorise acknowledging the interrupt?
	 */
	bool mayComplete;
};

/**
 * Type for sealed capabilities that authorise access to interrupts.
 */
typedef CHERI_SEALED(struct InterruptCapabilityState *) InterruptCapability;

/**
 * Helper macro to forward declare an interrupt capability.
 */
#define DECLARE_INTERRUPT_CAPABILITY(name)                                     \
	DECLARE_STATIC_SEALED_VALUE(                                               \
	  struct InterruptCapabilityState, scheduler, InterruptKey, name);

/**
 * Helper macro to define an interrupt capability.  The three arguments after
 * the name are the interrupt number and two boolean values indicating whether
 * it may be used with `interrupt_futex_get` and with `interrupt_complete`,
 * respectively.
 */
#define DEFINE_INTERRUPT_CAPABILITY(name, number, mayWait, mayComplete)        \
	DEFINE_STATIC_SEALED_VALUE(struct InterruptCapabilityState,                \
	                           scheduler,                                      \
	                           InterruptKey,                                   \
	                           name,                                           \
	                           number,                                         \
	                           mayWait,                                        \
	                           mayComplete);

/**
 * Helper macro to define an interrupt capability without a separate
 * declaration.  The arguments are the same as those for
 * `DEFINE_INTERRUPT_CAPABILITY`.
 */
#define DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(                               \
  name, number, mayWait, mayComplete)                                          \
	DECLARE_INTERRUPT_CAPABILITY(name);                                        \
	DEFINE_INTERRUPT_CAPABILITY(name, number, mayWait, mayComplete)

/**
 * Request the futex associated with an interrupt.  The argument is a sealed
 * capability to an `InterruptCapabilityState` structure that must have
 * `mayWait` flag set to true.  This is sealed with the `InterruptKey` type
 * exposed from the scheduler compartment.
 *
 * Returns `nullptr` on failure.
 */
__cheri_compartment("scheduler") const uint32_t *interrupt_futex_get(
  InterruptCapability interruptcapability);

/**
 * Acknowledge the end of handling an interrupt.  The argument is a sealed
 * capability to an `InterruptCapabilityState` structure that must have
 * `mayWait` flag set to true.  This is sealed with the `InterruptKey` type
 * exposed from the scheduler compartment.
 *
 * Returns 0 on success or `-EPERM` if the argument does not authorise this
 * operation.
 */
__cheri_compartment("scheduler") int interrupt_complete(
  InterruptCapability interruptcapability);

#if defined(__cplusplus)

#	include <futex.h>

/**
 * Packaged common design pattern for waiting for an event using an interrupt
 * futex.
 *
 * Invokes the `check` callback and `futex_timed_wait(timeout, futex,...)` in a
 * loop until either returns nonzero then returns that value. The loop is
 * structured to use the interrupt futex correctly to avoid missed wakeups.
 *
 * The `enableInterrupts` callback is invoked if the first call to `check`
 * returns zero and is expected to ensure that the given `futex` will increment
 * at least when `check`'s return value would (meaningfully) change.  In the
 * case where the event source is not shared across threads, it is likely
 * acceptable for this callback to disable its other interrupt sources.
 */
template<typename EnableInterrupts, typename Check>
    requires std::is_invocable_r_v<void, EnableInterrupts> &&
	         std::is_invocable_v<Check> &&
	         std::convertible_to<std::invoke_result_t<Check>, int>
__always_inline int interrupt_wait(TimeoutArgument          timeout,
                                   const volatile uint32_t *futex,
                                   EnableInterrupts         enableInterrupts,
                                   Check                    check)
{
	int      res;
	uint32_t lastFutexValue;

	// Probe the status register without doing any IRQ dance first
	if ((res = static_cast<int>(check())) != 0)
	{
		return res;
	}

	enableInterrupts();

	do
	{
		// Prevent prior writes from reordering after the futex read below
		__c11_atomic_signal_fence(__ATOMIC_RELEASE);
		lastFutexValue = *futex;
		// Prevent subsequent loads from reordering prior to the futex read
		__c11_atomic_signal_fence(__ATOMIC_ACQUIRE);

		if ((res = static_cast<int>(check())) != 0)
		{
			return res;
		}
	} while ((res = futex_timed_wait(timeout, futex, lastFutexValue)) == 0);

	return res;
}

#endif

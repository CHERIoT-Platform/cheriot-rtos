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
 * Packaged common design pattern for handlers of edge-triggered interrupts for
 * seamless transitions between polling and waiting for an interrupt.  As long
 * as events arrive quickly, this pattern avoids interaction with the scheduler
 * and interrupt controller.  Only once processing looks to have no more events
 * to process will the thread call in to the scheduler to atomically notify the
 * interrupt controller and then sleep until the next event (interrupt) arrives.
 *
 * Roughly, this method invokes `body()` every time `check()` returns `true`.
 * When `check` returns `false`, this method instead enters the scheduler to
 * atomically, indefinitely wait for the arrival of an interrupt that notifies
 * `futex`.  If such an interrupt arrives between the invocation of `check()`
 * and this thread sleeping, the attempted wait will instead immediately return.
 *
 * In fact, the return type of `check` is templated; if it is not `bool`, then
 * the result from `check()` first interpreted by explicit conversion to `bool`
 * to determine whether or not `body` is invoked and, if `true`, then it is
 * passed as an argument to `body()`.  This is useful for communicating, for
 * example, the observed fill level of a FIFO that will be processed in `body`.
 *
 * `body` may have return type `void` or `bool`.  In the latter case, if any
 * invocation of `body` returns `false`, this method returns immediately.
 *
 * The observed value of the futex is not passed to check or body, as its exact
 * value is unlikely to be of interest.
 */
template<typename T, typename Check>
    requires std::is_nothrow_invocable_r_v<T, Check> &&
	         requires { static_cast<bool>(std::declval<T>()); }
__always_inline void
interrupt_handler_with_futex(const uint32_t *futex, Check check, auto body)
{
	while (true)
	{
		uint32_t futexValue;
		T        checkResult;

		for (futexValue = *futex;
		     (checkResult = check()), static_cast<bool>(checkResult);
		     futexValue = *futex)
		{
			if constexpr (std::is_same_v<bool, T>)
			{
				if constexpr (std::is_nothrow_invocable_r_v<bool,
				                                            decltype(body)>)
				{
					if (!body())
					{
						return;
					}
				}
				else if constexpr (std::is_nothrow_invocable_r_v<
				                     void,
				                     decltype(body)>)
				{
					body();
				}
				else
				{
					static_assert(
					  false,
					  "Interrupt handler body must take no arguments and"
					  " return bool or void");
				}
			}
			else
			{
				if constexpr (std::is_nothrow_invocable_r_v<bool,
				                                            decltype(body),
				                                            T>)
				{
					if (!body(checkResult))
					{
						return;
					}
				}
				else if constexpr (std::is_nothrow_invocable_r_v<void,
				                                                 decltype(body),
				                                                 T>)
				{
					body(checkResult);
				}
				else
				{
					static_assert(
					  false,
					  "Interrupt handler body must take a T and return bool or"
					  " void");
				}
			}
		}
		futex_wait(futex, futexValue);
	}
}
#endif

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
/**
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

/**
 * The names of interrupts.  This is populated from the interrupts array in the
 * board configuration JSON and allows code to refer to interrupt numbers by
 * symbolic values.
 */
enum InterruptName : uint16_t
{
#ifdef CHERIOT_INTERRUPT_NAMES
	CHERIOT_INTERRUPT_NAMES
#endif
};

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
struct SObjStruct;

/**
 * Request the futex associated with an interrupt.  The argument is a sealed
 * capability to an `InterruptCapabilityState` structure that must have
 * `mayWait` flag set to true.  This is sealed with the `InterruptKey` type
 * exposed from the scheduler compartment.
 *
 * Returns `nullptr` on failure.
 */
__cheri_compartment("scheduler") const uint32_t *interrupt_futex_get(
  struct SObjStruct *);

/**
 * Acknowledge the end of handling an interrupt.  The argument is a sealed
 * capability to an `InterruptCapabilityState` structure that must have
 * `mayWait` flag set to true.  This is sealed with the `InterruptKey` type
 * exposed from the scheduler compartment.
 *
 * Returns 0 on success or `-EPERM` if the argument does not authorise this
 * operation.
 */
__cheri_compartment("scheduler") int interrupt_complete(struct SObjStruct *);

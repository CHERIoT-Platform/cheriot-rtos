// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
/**
 * @file multiwaiter.h
 *
 * This file provides interfaces to the multi-waiter system.  A multi-waiter
 * object (or 'multiwaiter') is an object that allows a calling thread to
 * suspend execution until one of a set of events has occurred.
 *
 * The CHERIoT multiwaiter system is designed to avoid allocation (or
 * interaction with the allocator) on the fast path.  The scheduler may not
 * capture any of the arguments to the multiwaiter's wait call and expose them
 * to other threads unless they are heap allocated.  It must also be robust in
 * the presence of malicious code that attempts to concurrently mutate any data
 * structures while sleeping.
 *
 * The multiwaiter object is allocated with space to wait for *n* objects, up
 * to a fixed limit.  Each wait call provides a set of things to wait on and
 * will suspend until either one occurs or a timeout is reached.  On return,
 * the caller-provided list will be updated.  This list can be allocated on the
 * stack: the scheduler does not need to hold a copy of it between calls or
 * write to it from another thread.
 *
 * The multiwaiter should be used sparingly.  In the worst case, it can add a
 * linear-complexity overhead on all wake events.  Memory overhead and code
 * size have been the key optimisation goals for this design.  Unlike systems
 * such as `kqueue`, scalability has not been a priority in this design because
 * expected number of waited objects is small and so is the number of threads.
 */
#include <compartment.h>
#include <stdlib.h>
#include <timeout.h>

/**
 * Structure describing a change to the set of managed event sources for an
 * event waiter.
 */
struct EventWaiterSource
{
	/**
	 * A pointer to the event source.  This is the futex that is monitored for
	 * the multiwaiter.
	 */
	void *eventSource;
	/**
	 * Event value.  This field is modified during the wait
	 * call.
	 *
	 * This indicates the value to compare the futex word against.  If they
	 * mismatch, the event fires immediately.
	 *
	 * On return, this is set to 1 if the futex is signaled, 0 otherwise.
	 */
	uint32_t value;
};

/**
 * Structure used for the MultiWaiter inside the scheduler.
 */
struct MultiWaiterInternal;

/**
 * Opaque type for multiwaiter objects.  Callers will always see this as a
 * sealed object.
 */
typedef CHERI_SEALED(struct MultiWaiterInternal *) MultiWaiter;

/**
 * Create a multiwaiter object.  This is a stateful object that can wait on at
 * most `maxItems` event sources.
 */
[[cheriot::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  multiwaiter_create(Timeout            *timeout,
                     AllocatorCapability heapCapability,
                     MultiWaiter        *ret,
                     size_t              maxItems);

/**
 * Destroy a multiwaiter object.
 */
[[cheriot::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  multiwaiter_delete(AllocatorCapability heapCapability, MultiWaiter mw);

/**
 * Wait for events.  The first argument is the multiwaiter to wait on.  New
 * events can optionally be added by providing an array of `newEventsCount`
 * elements as the `newEvents` argument.
 *
 * Return values:
 *
 *  - On success, this function returns 0.
 *  - If the arguments are invalid, this function returns -EINVAL.
 *  - If the timeout is reached without any events being triggered then this
 *    returns -ETIMEOUT.
 */
[[cheriot::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  multiwaiter_wait(Timeout                  *timeout,
                   MultiWaiter               waiter,
                   struct EventWaiterSource *events,
                   size_t                    newEventsCount);

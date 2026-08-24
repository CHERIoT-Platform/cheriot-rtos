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
 * to a fixed limit.  Each wait call provides a set of futexes to wait on, along
 * with their expected values, and will suspend if all currently hold their
 * expected values at the time of the call.  It will then sleep until either one
 * (or more) of the futexes is woken or a timeout is reached.  On return, the
 * caller-provided list will be updated, replacing the expected values with 0
 * if this futex is not woken or 1 if it is.  Note that this, like the
 * single-futex futex-wait API, is inherently racy: a futex value may be changed
 * and the futex notified immediately after the multiwaiter returns and before
 * the caller has inspected the results.
 *
 * The list of futexes can be allocated on the stack: the scheduler does not
 * need to hold a copy of it between calls or write to it from another thread.
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
  multiwaiter_create(TimeoutArgument     timeout,
                     AllocatorCapability heapCapability,
                     MultiWaiter        *ret,
                     size_t              maxItems);

/**
 * Destroy a multiwaiter object.
 */
[[cheriot::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  multiwaiter_delete(AllocatorCapability heapCapability, MultiWaiter mw);

/**
 * Wait for events.  The first argument is the multiwaiter to wait on.  Events
 * are added by providing an array of `newEventsCount` elements as the
 * `newEvents` argument.  The value of `newEventsCount` may not exceed the value
 * of `maxItems` passed to `multiwaiter_create` when `waiter` was created.
 *
 * Each entry in `newEvents` is a pair of a pointer to a futex word and the
 * expected value of that futex word.  The futex values may be modified by
 * another thread while the caller is preparing the events array.  If this
 * happens then the multiwaiter will return immediately.  If not, then the
 * multiwaiter will block until either one of the futexes is signalled with
 * `futex_wake` or until the timeout (specified in `timeout`) expires.
 *
 * On successful return, the `value` field for each element in `newEvents` will
 * be set to zero or one.  A value of one indicates that the futex identified
 * by the corresponding `eventSource` field was signalled, or had changed
 * before the initial check.  A value of zero indicates that this futex was not
 * signalled, but does not imply that the event did not happen immediately
 * after return.
 *
 * Callers must reinitialise the `value` field of each event source before each
 * call.  Typically, this is done by setting it to a value that was read from
 * the futex word *before* checking any other state that the futex is
 * associated with.  For example, if a futex corresponds to an interrupt, the
 * caller should read the futex word, check that the device has no pending
 * events, and then use the read value of the futex word in the multiwaiter.
 * This ensures that events that happen while preparing the multiwaiter state
 * are counted.
 *
 * Return values:
 *
 *  - On success, this function returns `0`.
 *  - If the arguments are invalid, this function returns `-EINVAL`.
 *  - If the timeout is reached without any events being triggered then this
 *    returns `-ETIMEDOUT`.
 */
[[cheriot::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  multiwaiter_wait(TimeoutArgument           timeout,
                   MultiWaiter               waiter,
                   struct EventWaiterSource *events,
                   size_t                    newEventsCount);

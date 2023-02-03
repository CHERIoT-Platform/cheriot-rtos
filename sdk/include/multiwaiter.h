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
#include <timeout.h>

/**
 * The kind of event source to wait for in a multiwaiter.
 */
enum EventWaiterKind
{
	/// Event source is a message queue.
	EventWaiterQueue,
	/// Event source is an event channel.
	EventWaiterEventChannel,
	/// Event source is a futex.
	EventWaiterFutex
};

enum [[clang::flag_enum]] EventWaiterQueueFlags{
  /// Notify when the queue is not full.
  EventWaiterQueueSendReady = (1 << 0),
  /// Notify when the queue is not empty.
  EventWaiterQueueReceiveReady = (1 << 1)};

enum [[clang::flag_enum]] EventWaiterEventChannelFlags{
  /// Automatically clear the bits we waited on.
  EventWaiterEventChannelClearOnExit = (1 << 24),
  /// Notify when all bits were set.
  EventWaiterEventChannelWaitAll = (1 << 26)};

/**
 * Structure describing a change to the set of managed event sources for an
 * event waiter.
 */
struct EventWaiterSource
{
	/**
	 * A pointer to the event source.  For a futex, this should be the memory
	 * address.  For other sources, it should be a pointer to an object of the
	 * corresponding type.
	 */
	void *eventSource;
	/**
	 * The kind of the event source.  This must match the pointer type.
	 */
	enum EventWaiterKind kind;
	/**
	 * Event-specific configuration.  This field is modified during the wait
	 * call.  The interpretation of this depends on `kind`:
	 *
	 * - `EventWaiterQueue`: this contains a bitmap of `EventWaiterQueueFlags`
	 *   values indicating the events to watch for.  On return, the bits for
	 *   the values that have been set will be stored.
	 * - `EventWaiterEventChannel`: The low 24 bits contain the bits to
	 *   monitor, the top bit indicates whether this event is triggered if all
	 *   of the bits are set (true) or some of them (false).  On return, this
	 *   contains the bits that have been set during the call.
	 * - `EventWaiterFutex`: This indicates the value to compare the futex word
	 *   against.  If they mismatch, the event fires immediately.
	 *
	 * If waiting for a futex, signal the event immediately if the value
	 * does not match.  On return, this is set to 1 if the futex is
	 * signaled, 0 otherwise.
	 */
	uint32_t value;
};

/**
 * Opqaue type for multiwaiter objects.  Callers will always see this as a
 * sealed object.
 */
struct MultiWaiter;

/**
 * Create a multiwaiter object.  This is a stateful object that can wait on at
 * most `maxItems` event sources.
 */
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  multiwaiter_create(struct MultiWaiter **ret, size_t maxItems);

/**
 * Destroy a multiwaiter object.
 */
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  multiwaiter_delete(struct MultiWaiter *mw);

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
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  multiwaiter_wait(struct MultiWaiter       *waiter,
                   struct EventWaiterSource *events,
                   size_t                    newEventsCount,
                   Timeout                  *timeout);

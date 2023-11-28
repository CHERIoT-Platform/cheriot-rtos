// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include "thread.h"
#include <errno.h>
#include <multiwaiter.h>
#include <riscvreg.h>
#include <stdint.h>
#include <type_traits>

namespace
{
	using namespace CHERI;

	/**
	 * Structure describing state for waiting for a single event source.
	 *
	 * This is roughly analogous to a knote in kqueue: the structure that holds
	 * state related to a specific event trigger.
	 */
	struct EventWaiter
	{
		/**
		 * The object (queue, event channel, or `uint32_t` address for futexes)
		 * that is monitored by this event waiter.
		 */
		void *eventSource = nullptr;
		/**
		 * Event-type value.
		 */
		uint32_t eventValue = 0;
		/**
		 * Event-type-specific flags.
		 */
		unsigned flags : 6 = 0;
		/**
		 * Value indicating the events that have occurred.  The zero value is
		 * reserved to indicate that this event has not been triggered,
		 * subclasses are responsible for defining interpretations of others.
		 */
		unsigned readyEvents : 24 = 0;
		/**
		 * Set some of the bits in the readyEvents field.  Any bits set in
		 * `value` will be set, in addition to any that are already set.
		 */
		void set_ready(unsigned value)
		{
			Debug::Invariant((value & 0xFF000000) == 0,
			                 "{} is out of range for a delivered event",
			                 value);
			readyEvents |= value;
		}
		/**
		 * Returns true if this event has fired, false otherwise.
		 */
		bool has_fired()
		{
			return readyEvents != 0;
		}

		/**
		 * Reset method.  Takes a pointer to the futex word and the
		 * user-provided value describing when it should fire.
		 */
		bool reset(uint32_t *address, uint32_t value)
		{
			eventSource = reinterpret_cast<void *>(
			  static_cast<uintptr_t>(Capability{address}.address()));
			eventValue  = value;
			flags       = 0;
			readyEvents = 0;
			if (*address != value)
			{
				set_ready(1);
				return true;
			}
			return false;
		}

		/**
		 * Trigger method that is called when a futex is notified.  Checks for
		 * matches against the address.
		 */
		bool trigger(ptraddr_t address)
		{
			ptraddr_t sourceAddress = Capability{eventSource}.address();
			if (sourceAddress != address)
			{
				return false;
			}
			set_ready(1);
			return true;
		}
	};

	static_assert(
	  sizeof(EventWaiter) == (2 * sizeof(void *)),
	  "Each waited event should consume only two pointers worth of memory");

	/**
	 * Multiwaiter object.  This contains space for all of the triggers.
	 */
	class MultiWaiterInternal : public Handle
	{
		public:
		/**
		 * Type marker used for `Handle::unseal_as`.
		 */
		static constexpr auto TypeMarker = Handle::Type::MultiWaiter;

		private:
		/**
		 * We place a limit on the number of waiters in an event queue to
		 * bound the time spent traversing them.
		 */
		static constexpr size_t MaxMultiWaiterSize = 8;

		/**
		 * The maximum number of events in this multiwaiter.
		 */
		const uint8_t Length;
		/**
		 * The current number of events in this multiwaiter.
		 */
		uint8_t usedLength = 0;

		/**
		 * Multiwaiters are added to a list in between being triggered
		 */
		MultiWaiterInternal *next = nullptr;

		/**
		 * The array of events that we're waiting for.  This is variable sized
		 * and must be the last field of the structure.
		 */
		EventWaiter events[];

		public:
		/**
		 * Returns an iterator to the event waiters that this multiwaiter
		 * contains.
		 */
		EventWaiter *begin()
		{
			return events;
		}

		/**
		 * Returns an end iterator to the event waiters that this multiwaiter
		 * contains.
		 */
		EventWaiter *end()
		{
			return events + usedLength;
		}

		/**
		 * Returns the maximum number of event waiters that this is permitted
		 * to hold.
		 */
		size_t capacity()
		{
			return Length;
		}

		/**
		 * Returns the number of event waiters that this holds.
		 */
		size_t size()
		{
			return usedLength;
		}

		/**
		 * Factory method.  Creates a multiwaiter of the specified size.  On
		 * failure, sets `error` to the errno constant corresponding to the
		 * failure reason and return `nullptr`.
		 */
		static HeapObject<MultiWaiterInternal>
		create(Timeout           *timeout,
		       struct SObjStruct *heapCapability,
		       size_t             length,
		       int               &error)
		{
			static_assert(sizeof(MultiWaiterInternal) <= 2 * sizeof(void *),
			              "Header for event queue is too large");
			if (length > MaxMultiWaiterSize)
			{
				error = -EINVAL;
				return {};
			}
			void *q = heap_allocate(timeout,
			                        heapCapability,
			                        sizeof(MultiWaiterInternal) +
			                          (length * sizeof(EventWaiter)));
			if (q == nullptr)
			{
				error = -ENOMEM;
				return {};
			}
			error = 0;
			return {heapCapability, new (q) MultiWaiterInternal(length)};
		}

		/**
		 * Tri-state return from `set_events`.
		 */
		enum class EventOperationResult
		{
			/// Failure, report an error.
			Error,
			/// Success and an event fired already.
			Wake,
			/// Success but no events fired, sleep until one does.
			Sleep
		};

		/**
		 * Set the events provided by the user.  The caller is responsible for
		 * ensuring that `newEvents` is a valid and usable capability and that
		 * `count` is within the capacity of this object.
		 */
		EventOperationResult set_events(EventWaiterSource *newEvents,
		                                size_t             count)
		{
			// Has any event triggered yet?
			bool eventTriggered = false;
			// Reset the events that this contains.
			for (size_t i = 0; i < count; i++)
			{
				void *ptr     = newEvents[i].eventSource;
				auto *address = static_cast<uint32_t *>(ptr);
				if (!check_pointer<PermissionSet{Permission::Load}>(address))
				{
					return EventOperationResult::Error;
				}
				eventTriggered |= events[i].reset(address, newEvents[i].value);
			}
			usedLength = count;
			return eventTriggered ? EventOperationResult::Wake
			                      : EventOperationResult::Sleep;
		}

		/**
		 * Destructor, ensures that nothing is waiting on this.
		 */
		~MultiWaiterInternal()
		{
			// Remove from the pending-wake list
			remove_from_pending_wake_list();
			// If this is on any threads that it's waiting on.
			Thread::walk_thread_list(threads, [&](Thread *thread) {
				if (thread->multiWaiter == this)
				{
					thread->multiWaiter = nullptr;
					thread->ready(Thread::WakeReason::Timer);
				}
			});
		}

		/**
		 * Function to handle the end of a multi-wait operation.  This collects
		 * all of the results from each of the events and propagates them to
		 * the query list.  The caller is responsible for ensuring that
		 * `newEvents` is valid.
		 */
		bool get_results(EventWaiterSource *newEvents, size_t count)
		{
			// Remove ourself from the list of waiters.
			remove_from_pending_wake_list();
			// Collect all events that have fired.
			Debug::Assert(
			  count <= Length, "Invalid length {} > {}", count, Length);
			bool found = false;
			for (size_t i = 0; i < count; i++)
			{
				newEvents[i].value = events[i].readyEvents;
				found |= (events[i].readyEvents != 0);
			}
			return found;
		}

		/**
		 * Helper that should be called whenever an event of type `T` is ready.
		 * This will always notify any waiters that have already been woken but
		 * have not yet returned.  The `maxWakes` parameter can be used to
		 * restrict the number of threads that are woken as a result of this
		 * call.
		 */
		template<typename T>
		static uint32_t
		wake_waiters(T        source,
		             uint32_t info     = 0,
		             uint32_t maxWakes = std::numeric_limits<uint32_t>::max())
		{
			// Trigger any multiwaiters whose threads have been woken but which
			// have not yet been scheduled.
			for (auto *mw = wokenMultiwaiters; mw != nullptr; mw = mw->next)
			{
				mw->trigger(source);
			}
			// Look at any threads that are waiting on multiwaiters.  This
			// should happen after waking the multiwaiters so that we don't
			// visit multiwaiters twice
			uint32_t woken = 0;
			Thread::walk_thread_list(
			  threads,
			  [&](Thread *thread) {
				  if (thread->multiWaiter->trigger(source))
				  {
					  thread->ready(Thread::WakeReason::MultiWaiter);
					  woken++;
					  thread->multiWaiter->next = wokenMultiwaiters;
					  wokenMultiwaiters         = thread->multiWaiter;
				  }
			  },
			  [&]() { return woken >= maxWakes; });
			return woken;
		}

		/**
		 * Wait on this multi-waiter object until either the timeout expires or
		 * one or more events have fired.
		 */
		void wait(Timeout *timeout)
		{
			Thread *currentThread      = Thread::current_get();
			currentThread->multiWaiter = this;
			currentThread->suspend(timeout, &MultiWaiterInternal::threads);
			currentThread->multiWaiter = nullptr;
		}

		private:
		/**
		 * Helper to remove this object from the list maintained for
		 * multiwaiters that have been triggered but whose threads have not yet
		 * been scheduled.
		 */
		void remove_from_pending_wake_list()
		{
			MultiWaiterInternal **prev = &wokenMultiwaiters;
			while ((prev != nullptr) && (*prev != nullptr) && ((*prev) != this))
			{
				prev = &((*prev)->next);
			}
			if (prev != nullptr)
			{
				*prev = next;
			}
			next = nullptr;
		}
		/**
		 * Deliver an event from the source to all possible waiting events in
		 * this set.  This returns true if any of the event sources matches
		 * this multiwaiter and the thread should be awoken.
		 */
		template<typename T>
		bool trigger(T source, uint32_t info = 0)
		{
			bool shouldWake = false;
			for (auto &registeredSource : *this)
			{
				shouldWake |= registeredSource.trigger(source);
			}
			return shouldWake;
		}

		/**
		 * Private constructor, called only from the factory method (`create`).
		 */
		MultiWaiterInternal(size_t length) : Handle(TypeMarker), Length(length)
		{
		}

		/**
		 * Priority-sorted wait queue for threads that are blocked on a
		 * multiwaiter.
		 */
		static inline Thread *threads;

		/**
		 * List of multiwaiters whose threads have been woken but not yet run.
		 */
		static inline MultiWaiterInternal *wokenMultiwaiters = nullptr;
	};

} // namespace

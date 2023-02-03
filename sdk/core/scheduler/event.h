// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "multiwait.h"
#include "thread.h"
#include <debug.hh>
#include <errno.h>
#include <stddef.h>
#include <utility>
#include <utils.h>

namespace sched
{

	class Event final : private utils::NoCopyNoMove, public Handle
	{
		public:
		/**
		 * Type marker used for `Handle::unseal_as`.
		 */
		static constexpr auto TypeMarker = Handle::Type::Queue;

		/**
		 * Unseal this as an event, returning nullptr if it is not a sealed
		 * event.
		 */
		Event *unseal()
		{
			return unseal_as<Event>();
		}

		Event() : Handle(TypeMarker), bits({0, 0}) {}

		int bits_wait(uint32_t *retBits,
		              uint32_t  bitsToWait,
		              bool      clearOnExit,
		              bool      waitAll,
		              Timeout  *timeout)
		{
			Thread::EventBits temp = {.bits        = bitsToWait,
			                          .clearOnExit = clearOnExit,
			                          .waitForAll  = waitAll};
			Thread           *curr;

			if (bitsToWait == 0 || (bitsToWait >> 24) != 0)
			{
				return -EINVAL;
			}

			// Have the bits already been set? If so, events are pending and we
			// can return straight away.
			if (can_unblock(bits.bits, temp))
			{
				*retBits = bits.bits;
				if (clearOnExit)
				{
					bits.bits &= ~bitsToWait;
				}
				return 0;
			}
			if (!timeout->may_block())
			{
				*retBits = bits.bits;
				return -EWOULDBLOCK;
			}

			// Block this thread.
			curr                = Thread::current_get();
			curr->eventWaitBits = temp;
			curr->suspend(timeout, &waitList);

			// After waking up, curr->eventWaitBits contains the bits that woke
			// us up. In the case of timer expiry, these bits are cleared.
			*retBits = curr->eventWaitBits.bits;

			if (curr->eventWaitBits.bits == 0)
			{
				return -ETIMEDOUT;
			}
			return 0;
		}

		std::pair<int, bool> bits_set(uint32_t *retBits, uint32_t bitsToSet)
		{
			uint32_t newBits, finalBits;

			if ((bitsToSet >> 24) != 0)
			{
				return {-EINVAL, false};
			}

			finalBits = newBits = bits.bits | bitsToSet;
			bool shouldYield    = false;
			if (waitList != nullptr)
			{
				// Go through the list of blocked waiters and unblock.
				for (Thread *iter = waitList;;)
				{
					Thread *iterNext = iter->next;

					if (can_unblock(newBits, iter->eventWaitBits))
					{
						if (iter->eventWaitBits.clearOnExit)
						{
							finalBits &= ~iter->eventWaitBits.bits;
						}
						iter->eventWaitBits.bits = newBits;
						/*
						 * We must not use shouldYield = shouldYield || ready()
						 * here because we are unblocking the waiter regardless,
						 * and short-circuiting must be avoided.
						 */
						shouldYield |=
						  iter->ready(sched::Thread::WakeReason::Event);
					}

					iter = iterNext;
					BARRIER();
					if (waitList == nullptr || iter == waitList)
					{
						break;
					}
				}
			}
			bits.bits = finalBits;
			*retBits  = finalBits;

			auto wokeMultiwaiters =
			  sched::MultiWaiter::wake_waiters(this, newBits);

			return {0, shouldYield || wokeMultiwaiters};
		}

		int bits_get(uint32_t *retBits)
		{
			*retBits = bits.bits;

			return 0;
		}

		int bits_clear(uint32_t *retBits, uint32_t bitsToClear)
		{
			*retBits = bits.bits;
			if ((bitsToClear >> 24) != 0)
			{
				return -EINVAL;
			}
			bits.bits &= ~bitsToClear;

			return 0;
		}

		~Event()
		{
			// Unblock everyone.
			while (waitList)
			{
				Thread *blocker = waitList;

				__builtin_assume(blocker->sleepQueue != nullptr);
				Debug::Assert(
				  (blocker->sleepQueue != nullptr) &&
				    (*blocker->sleepQueue == waitList),
				  "Blocker is on the wrong wait list ({}, expected {}",
				  *blocker->sleepQueue,
				  waitList);
				blocker->ready(Thread::WakeReason::Delete);
			}
		}

		private:
		static_assert(sizeof(Thread::EventBits) == sizeof(uint32_t));

		/**
		 * Check whether a `newBits` satisfies the requirement for `waitBits`
		 * to be unblocked.  Returns true if so, false otherwise.
		 */
		bool can_unblock(uint32_t newBits, Thread::EventBits waitBits)
		{
			bool     waitAll     = waitBits.waitForAll;
			uint32_t waitBitsRaw = waitBits.bits;
			if (waitAll)
			{
				// newBits must have all the bits needed in waitBitsRaw.
				return (waitBitsRaw & newBits) == waitBitsRaw;
			}
			// If not waitAll, then any overlapping bit counts.
			return (waitBitsRaw & newBits) != 0;
		}

		Thread::EventBits bits;
		Thread           *waitList;
	};

	inline bool EventWaiter::trigger(Event *event, uint32_t bitsBeforeClear)
	{
		if ((kind != EventWaiterEventChannel) ||
		    (Capability{event} != eventSource))
		{
			return false;
		}
		uint32_t           maskedBits = bitsBeforeClear & eventValue;
		constexpr uint32_t WaitAll    = 0b100;
		constexpr uint32_t AutoClear  = 0b1;
		bool               canUnblock =
          (flags & WaitAll) ? maskedBits == eventValue : maskedBits != 0;
		if (canUnblock)
		{
			readyEvents |= maskedBits;
			if (flags & AutoClear)
			{
				uint32_t unused;
				int      rv = event->bits_clear(&unused, eventValue);
				Debug::Assert(rv == 0,
				              "eventValue should not contain flag bits");
			}
		}
		return canUnblock;
	}

	inline bool EventWaiter::reset(Event *event, uint32_t bits)
	{
		eventSource = event;
		// The low 24 bits describe the source
		eventValue = bits & 0xffffff;
		// For event channels, bit 0 in flags means clearOnExit and bit 2
		// means WaitForAll. This is just to mirror the bit field layout in
		// event channels.
		flags       = (bits >> 24) & 0b101;
		kind        = EventWaiterEventChannel;
		readyEvents = 0;
		uint32_t startBits;
		event->bits_get(&startBits);
		return trigger(event, startBits);
	};
} // namespace sched

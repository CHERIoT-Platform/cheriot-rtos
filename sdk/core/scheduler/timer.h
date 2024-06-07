// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "plic.h"
#include "thread.h"
#include <platform-timer.hh>
#include <stdint.h>
#include <tick_macros.h>
#include <utils.hh>

namespace
{
	/**
	 * Concept for the interface to setting the system timer.
	 */
	template<typename T>
	concept IsTimer = requires(uint32_t cycles)
	{
		{T::init()};
		{T::setnext(cycles)};
	};

	static_assert(
	  IsTimer<TimerCore>,
	  "Platform's timer implementation does not meet the required interface");

	/**
	 * Timer interface.  Provides generic timer functionality to the scheduler,
	 * wrapping the platform's timer device.
	 */
	class Timer final : private TimerCore
	{
		inline static uint64_t lastTickTime         = 0;
		inline static uint64_t zeroTickTime         = 0;
		inline static uint32_t accumulatedTickError = 0;

		public:
		/**
		 * Perform any setup necessary for the timer device.
		 */
		static void interrupt_setup()
		{
			static_assert(TIMERCYCLES_PER_TICK <= UINT32_MAX,
			              "Cycles per tick can't be represented in 32 bits. "
			              "Double check your platform config");
			init();
			zeroTickTime = time();
		}

		/**
		 * Expose the timer device's method for returning the current time.
		 */
		using TimerCore::time;

		/**
		 * Update the timer to fire the next timeout for the thread at the
		 * front of the queue, or disable the timer if there are no threads
		 * blocked with a timeout and no threads with the same priority.
		 *
		 * The scheduler is a simple RTOS scheduler that does not allow any
		 * thread to run if a higher-priority thread is runnable.  This means
		 * that we need a timer interrupt in one of two situations:
		 *
		 *  - We have a thread of the same priority as the current thread and
		 *    we are going to round-robin schedule it.
		 *  - We have a thread of a higher priority than the current thread
		 *    that is currently sleeping on a timeout and need it to preempt the
		 *    current thread when its timeout expires.
		 *
		 * We currently over approximate the second condition by making the
		 * timer fire independent of the priority.  If this is every changed,
		 * some care must be taken to ensure that dynamic priority propagation
		 * via priority-inheriting futexes behaves correctly.
		 *
		 * This should be called after scheduling has changed the list of
		 * waiting threads.
		 */
		static void update()
		{
			auto *thread             = Thread::current_get();
			bool  waitingListIsEmpty = ((Thread::waitingList == nullptr) ||
                                       (Thread::waitingList->expiryTime == -1));
			bool  threadHasNoPeers =
			  (thread == nullptr) || (!thread->has_priority_peers());
			if (waitingListIsEmpty && threadHasNoPeers)
			{
				clear();
			}
			else
			{
				static constexpr uint64_t DistantFuture =
				  std::numeric_limits<uint64_t>::max();
				uint64_t nextTick  = threadHasNoPeers
				                       ? DistantFuture
				                       : time() + TIMERCYCLES_PER_TICK;
				uint64_t nextTimer = waitingListIsEmpty
				                       ? DistantFuture
				                       : Thread::waitingList->expiryTime;
				setnext(std::min(nextTick, nextTimer));
			}
		}

		/**
		 * Wake any threads that were sleeping until a timeout before the
		 * current time.  This also wakes yielded threads if there are no
		 * runnable threads.
		 *
		 * This should be called when a timer interrupt fires.
		 */
		static void expiretimers()
		{
			uint64_t now = time();
			Thread::ticksSinceBoot =
			  (now - zeroTickTime) / TIMERCYCLES_PER_TICK;
			if (Thread::waitingList == nullptr)
			{
				return;
			}
			for (Thread *iter = Thread::waitingList;;)
			{
				if (iter->expiryTime <= now)
				{
					Thread *iterNext = iter->timerNext;

					iter->ready(Thread::WakeReason::Timer);
					iter = iterNext;
					if (Thread::waitingList == nullptr ||
					    iter == Thread::waitingList)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			// If there are not runnable threads, try to wake a yielded thread
			if (!Thread::any_ready())
			{
				// Look at the first thread.  If it is not yielding, there may
				// be another thread behind it that is, but that's fine.  We
				// don't want to encounter situations where (with a
				// high-priority A and a low-priority B):
				//
				// 1. A yields for 5 ticks.
				// 2. B starts and does a blocking operation (e.g. try_lock)
				//    with a 1-tick timeout.
				// 3. A wakes up and prevents B from running even though we're
				//    still in its 5-tick yield period.
				if (Thread *head = Thread::waitingList)
				{
					if (head->is_yielding())
					{
						Debug::log("Woke thread {} {} cycles early",
						           head->id_get(),
						           int64_t(head->expiryTime) - now);
						head->ready(Thread::WakeReason::Timer);
					}
				}
			}
		}
	};

	uint64_t expiry_time_for_timeout(uint32_t timeout)
	{
		if (timeout == -1)
		{
			return -1;
		}
		return Timer::time() + (timeout * TIMERCYCLES_PER_TICK);
	}
} // namespace

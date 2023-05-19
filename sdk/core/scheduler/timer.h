// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

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
} // namespace

static_assert(
  IsTimer<TimerCore>,
  "Platform's timer implementation does not meet the required interface");

namespace sched
{

	class Timer final : private TimerCore
	{
		public:
		static void interrupt_setup()
		{
			static_assert(TIMERCYCLES_PER_TICK <= UINT32_MAX,
			              "Cycles per tick can't be represented in 32 bits. "
			              "Double check your platform config");
			init();
			setnext(TIMERCYCLES_PER_TICK);
		}

		static void do_interrupt()
		{
			++Thread::ticksSinceBoot;

			expiretimers();
			setnext(TIMERCYCLES_PER_TICK);
		}

		private:
		static void expiretimers()
		{
			if (Thread::waitingList == nullptr)
			{
				return;
			}
			for (Thread *iter = Thread::waitingList;;)
			{
				if (iter->expiryTime <= Thread::ticksSinceBoot)
				{
					Thread *iterNext = iter->timerNext;

					iter->ready(sched::Thread::WakeReason::Timer);
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
		}
	};
} // namespace sched

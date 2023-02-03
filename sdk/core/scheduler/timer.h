// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "thread.h"
#include <stdint.h>
#include <utils.h>

namespace
{
	class StandardClint : private utils::NoCopyNoMove
	{
		public:
		static void init()
		{
			// This needs to be csr_read(mhartid) if we support multicore
			// systems, but we don't plan to any time soon.
			constexpr uint32_t HartId = 0;

			auto setField = [&](auto &field, size_t offset, size_t size) {
				CHERI::Capability capability{MMIO_CAPABILITY(uint32_t, clint)};
				capability.address() += offset;
				capability.bounds() = size;
				field               = capability;
			};
			setField(pmtimer, StandardClint::ClintMtime, 2 * sizeof(uint32_t));
			setField(pmtimercmp,
			         StandardClint::ClintMtimecmp + HartId * 8,
			         2 * sizeof(uint32_t));
		}

		/**
		 * Set the timer up for the next timer interrupt. We need to:
		 * 1. read the current MTIME,
		 * 2. add tick amount of cycles to the current time,
		 * 3. write back the new value to MTIMECMP for the next interrupt.
		 * Because MTIMECMP is 64-bit and we are a 32-bit CPU, we need to:
		 * 1. ensure the high 32 bits are stable when we read the low 32 bits
		 * 2. ensure we write both halves in a way that will not produce
		 *    spurious interrupts.
		 * Note that CLINT does not need any action to acknowledge the
		 * interrupt. Writing to any half is enough to clear the interrupt,
		 * which is also why 2. is important.
		 */
		static void setnext()
		{
			/// the high 32 bits of the 64-bit MTIME register
			volatile uint32_t *pmtimercmphigh = pmtimercmp + 1;
			/// the low 32 bits
			volatile uint32_t *pmtimerhigh = pmtimer + 1;
			uint32_t           curmtimehigh, curmtime, curmtimenew;

			// Read the current time. Loop until the high 32 bits are stable.
			do
			{
				curmtimehigh = *pmtimerhigh;
				curmtime     = *pmtimer;
			} while (curmtimehigh != *pmtimerhigh);

			// Add tick cycles to current time. Handle carry bit.
			if (__builtin_add_overflow(
			      curmtime, TIMERCYCLES_PER_TICK, &curmtimenew))
			{
				curmtimehigh++;
			}

			// Write the new MTIMECMP value, at which the next interrupt fires.
			*pmtimercmphigh = -1; // Prevent spurious interrupts.
			*pmtimercmp     = curmtimenew;
			*pmtimercmphigh = curmtimehigh;
		}

		private:
		static constexpr size_t ClintMtimecmp = 0x4000U;
		static constexpr size_t ClintMtime    = 0xbff8U;

		static inline volatile uint32_t *pmtimercmp;
		static inline volatile uint32_t *pmtimer;
	};
} // namespace

namespace sched
{
#if defined(FLUTE) || defined(SAIL)
	using TimerCore = StandardClint;
#else
#	error "Unsupported timer source"
#endif

	class Timer final : private TimerCore
	{
		public:
		static void interrupt_setup()
		{
			static_assert(TIMERCYCLES_PER_TICK <= UINT32_MAX,
			              "Cycles per tick can't be represented in 32 bits. "
			              "Double check your platform config");
			init();
			setnext();
		}

		static void do_interrupt()
		{
			++Thread::ticksSinceBoot;

			expiretimers();
			setnext();
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

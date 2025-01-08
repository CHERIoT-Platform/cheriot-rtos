// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cheri.hh>
#include <stdint.h>
#include <utils.hh>

/**
 * Driver for using the (old) RISC-V Core-Local Interrupt (CLINT)
 * controller (as opposed to the newer CLIC) to provide timer signals.
 * Note that CLIC can run in CLINT-compatible mode, so we don't need to
 * support CLIC for a while, it has a lot of features that are overkill for
 * most embedded devices.
 */
class StandardClint : private utils::NoCopyNoMove
{
	public:
	/**
	 * Initialise the interface.
	 */
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

	static uint64_t time()
	{
		// The timer is little endian, so the high 32 bits are after the low 32
		// bits. We can't do atomic 64-bit loads and so we have to read these
		// separately.
		volatile uint32_t *timerHigh = pmtimer + 1;
		uint32_t           timeLow, timeHigh;

		// Read the current time. Loop until the high 32 bits are stable.
		do
		{
			timeHigh = *timerHigh;
			timeLow  = *pmtimer;
		} while (timeHigh != *timerHigh);
		return (static_cast<uint64_t>(timeHigh) << 32) | timeLow;
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
	static void setnext(uint64_t nextTime)
	{
		/// the high 32 bits of the 64-bit MTIME register
		volatile uint32_t *pmtimercmphigh = pmtimercmp + 1;
		uint32_t           curmtimehigh, curmtime, curmtimenew;

		// Write the new MTIMECMP value, at which the next interrupt fires.
		*pmtimercmphigh = -1; // Prevent spurious interrupts.
		*pmtimercmp     = nextTime;
		*pmtimercmphigh = nextTime >> 32;
	}

	static void clear()
	{
		volatile uint32_t *pmtimercmphigh = pmtimercmp + 1;
		*pmtimercmphigh                   = -1; // Prevent spurious interrupts.
	}

	private:
#ifdef IBEX_SAFE
	/**
	 * The Ibex-SAFE platform doesn't implement a complete CLINT, only the
	 * timer part (which is all that we use).  Its offsets within the CLINT
	 * region are different.
	 */
	static constexpr size_t ClintMtime    = 0x10;
	static constexpr size_t ClintMtimecmp = 0x18;
#else
	/**
	 * The standard RISC-V CLINT is a large memory-mapped device and places the
	 * timers a long way in.
	 */
	static constexpr size_t ClintMtimecmp = 0x4000U;
	static constexpr size_t ClintMtime    = 0xbff8U;
#endif

	static inline volatile uint32_t *pmtimercmp;
	static inline volatile uint32_t *pmtimer;
};

using TimerCore = StandardClint;

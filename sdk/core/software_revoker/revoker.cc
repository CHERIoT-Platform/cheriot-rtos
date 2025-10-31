// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "../allocator/software_revoker.h"
#include <array>
#include <cheri.hh>
#include <debug.hh>
#include <utility>

using CHERI::Capability;
using CHERI::Permission;

/**
 * We need an array of the allocations that provide the globals at the
 * start of our PCC, but the compiler doesn't currently provide a good way of
 * doing this, so do it with an assembly stub for loading the capabilities.
 */
__asm__("	.section .text, \"ax\", @progbits\n"
        "	.p2align 3\n"
        "globals:\n"
        "	.zero 1*8\n"
        "	.globl get_globals\n"
        "get_globals:\n"
        "	sll        a0, a0, 3\n"
        "1:\n"
        "	auipcc     ca1, %cheriot_compartment_hi(globals)\n"
        "	cincoffset ca1, ca1, %cheriot_compartment_lo_i(1b)\n"
        "	cincoffset ca1, ca1, a0\n"
        "	clc        ca0, 0(ca1)\n"
        "	cjr        cra\n");

extern "C" volatile void *volatile *get_globals(int idx);

namespace
{
	/**
	 * The index of the current range to scan.  Must be 0 or -1.
	 */
	int currentRange;
	/**
	 * The current offset within the scanned range, in pointer-sized units.
	 */
	size_t offset;
	/**
	 * The length of the region that's being scanned, in pointer-sized units.
	 */
	size_t length;
	/**
	 * The current revocation epoch.  If the low bit is 1, revocation is
	 * running.
	 */
	uint32_t epoch;

	/**
	 * The current state of the revoker's state machine.
	 */
	enum class State
	{
		/**
		 * Revocation is not currently happening.
		 */
		NotRunning,
		/**
		 * The revoker is scanning.
		 */
		Scanning
	} state;

	/**
	 * Advance the state machine to the next state.
	 */
	std::pair<int, State> next()
	{
		switch (state)
		{
			case State::NotRunning:
				return {0, State::Scanning};
			case State::Scanning:
				return {-1, State::NotRunning};
		}
	}

	/**
	 * The number of capabilities to scan per tick.  This probably needs some
	 * tuning.  Invoking the revoker costs around 400 cycles on Flute, so we're
	 * likely to be spending about half of our total time on domain transitions
	 * with a value <100.
	 */
	static constexpr size_t TickSize = 4096;

	/**
	 * Advance the state machine to the next state.
	 */
	void advance()
	{
		// If we're starting a revocation pass, increment the epoch counter (it
		// should now be odd).
		if (state == State::NotRunning)
		{
			epoch++;
			assert((epoch & 1) == 1);
		}
		// Find the next state.
		auto [nextRange, nextState] = next();
		currentRange                = nextRange;
		offset                      = 0;
		// If we have a new range, set the length to something sensible.
		if (nextRange != -1)
		{
			length = __builtin_cheri_length_get(get_globals(currentRange)) /
			         sizeof(void *);
		}
		state = nextState;
		// If we've finished a run, increment the epoch counter (it should now
		// be even).
		if (state == State::NotRunning)
		{
			epoch++;
			assert((epoch & 1) == 0);
		}
	}

	/**
	 * Scan a fixed-size range of the current memory region.
	 */
	void scan_range()
	{
		size_t end     = offset + std::min(length - offset, TickSize);
		auto   current = get_globals(currentRange);
		// With interrupts disabled, loading and storing a capability will
		// clear the tag on anything that has been revoked via the load
		// barrier.
		for (int i = offset; i < end; i++)
		{
			current[i] = current[i];
		}
		// Record the amount that we've scanned.
		offset = end;
		// Advance to the next state if we've finished scanning this range.
		if (offset == length)
		{
			advance();
		}
	}

} // namespace

int revoker_tick()
{
	// If we've been asked to run, make sure that we're running.
	if (state == State::NotRunning)
	{
		advance();
	}
	// Do some work.
	scan_range();

	return 0;
}

const uint32_t *revoker_epoch_get()
{
	Capability<uint32_t> epochPtr{&epoch};
	epochPtr.permissions() &= {Permission::Load, Permission::Global};
	return epochPtr;
}

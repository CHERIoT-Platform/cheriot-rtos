// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
/**
 * @file timeout.h
 *
 * This file contains the types used for timeouts across scheduler APIs.
 */

#include <cdefs.h>
#include <stdint.h>

/**
 * Quantity used for measuring time for timeouts.  The unit is scheduler ticks.
 */
typedef uint32_t Ticks;

/// Value indicating an unbounded timeout.
static __if_cxx(constexpr) const Ticks UnlimitedTimeout = UINT32_MAX;

/**
 * Structure representing a timeout.  This is intended to allow a single
 * instance to be chained across blocking calls.
 *
 * Timeouts *may not be stored in the heap*.  Handling timeout structures that
 * may disappear between sleeping and waking is very complicated and would
 * impact a lot of fast paths.  Instead, most functions that take a timeout
 * will simply fail if the timeout is on the heap.
 */
typedef struct Timeout
{
	/**
	 * The time that has elapsed during blocking operations for this timeout
	 * structure.  This should be initialised to 0.  It may exceed the initial
	 * value of `remaining` if a higher-priority thread preempts the blocking
	 * thread.
	 */
	Ticks elapsed __if_cxx(= 0);
	/**
	 * The remaining time.  This is clamped at 0 on subtraction.
	 */
	Ticks remaining;
#ifdef __cplusplus
	/**
	 * Constructor, initialises this structure to allow `time` ticks to
	 *  elapse.
	 */
	Timeout(Ticks time) : remaining(time) {}

	/**
	 * Constructor that initialises both fields, should be used only for
	 * initialiser-list initialisation in code that needs to compile as both C
	 * and C++.
	 */
	Timeout(Ticks elapsed, Ticks remaining)
	  : elapsed(elapsed), remaining(remaining)
	{
	}

	/**
	 * Update this timeout if `time` ticks have elapsed.  This function
	 * saturates the values on overflow.
	 */
	inline void elapse(Ticks time)
	{
		if (__builtin_add_overflow(time, elapsed, &elapsed))
		{
			elapsed = UnlimitedTimeout;
		}
		if (__builtin_sub_overflow(remaining, time, &remaining))
		{
			remaining = 0;
		}
	}

	/**
	 * Helper indicating whether the owner of this timeout may block.
	 */
	bool may_block()
	{
		return remaining > 0;
	}
#endif
} Timeout;

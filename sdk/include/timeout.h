// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
/**
 * @file timeout.h
 *
 * This file contains the types used for timeouts across scheduler APIs.
 */

#include <cdefs.h>
#include <cheri-builtins.h>
#include <platform-time.h>
#include <stdint.h>

/**
 * Quantity used for measuring time for timeouts.  The unit is scheduler ticks.
 */
typedef uint32_t Ticks;

/// Value indicating an unbounded timeout for use with `Timeout`.
static __if_cxx(constexpr) const Ticks UnlimitedTimeout = UINT32_MAX;

/**
 * Type used for absolute timeouts.  These are times against the
 * platform-specific uptime timer.
 */
typedef uint64_t AbsoluteMonotonicTimeout;

/// Timeout value for unlimited blocking.
static __if_cxx(constexpr) const AbsoluteMonotonicTimeout TimeoutWaitForever =
  0xffff'ffff'ffff'ffff;

/// Timeout value for not blocking.
static __if_cxx(constexpr) const AbsoluteMonotonicTimeout TimeoutNoWait = 0;

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
	 * The remaining time. This is clamped at 0 on subtraction. A special
	 * value of `UnlimitedTimeout` can be set to represent an unlimited
	 * timeout.
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
		if (remaining != UnlimitedTimeout &&
		    __builtin_sub_overflow(remaining, time, &remaining))
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

/**
 * A union to hold timeouts that are passed down to end up in the scheduler.
 * These are one of two things:
 *
 *  - An absolute timeout, measured in cycles of some platform-specific timer
 *    (such as the `mtime` register) that increments at predictable rate.
 *  - A relative timeout counted in elapsed scheduler ticks.
 *
 * This is a tagged union, with the CHERI tag bit used to differentiate between
 * the two cases.
 */
typedef union __if_c(__attribute__((transparent_union))) __TimeoutArgument
{
	/**
	 * An absolute timeout.
	 */
	AbsoluteMonotonicTimeout absoluteTimeout;
	/**
	 * A relative timeout.
	 */
	Timeout *relativeTimeout;

#ifdef __cplusplus
	/// Permit implicit construction from absolute timeouts.
	__TimeoutArgument(AbsoluteMonotonicTimeout absoluteTimeout)
	  : absoluteTimeout(absoluteTimeout)
	{
	}
	/// Permit implicit construction from relative timeouts.
	__TimeoutArgument(Timeout *relativeTimeout)
	  : relativeTimeout(relativeTimeout)
	{
	}

	/**
	 * Returns true if this is a relative timeout, false otherwise.
	 */
	bool is_relative()
	{
		return cheri_is_valid(relativeTimeout);
	}

	/**
	 * Returns true if this is a valid timeout, false otherwise.
	 */
	bool is_valid();

	/**
	 * Returns true if the timeout is in the future (and therefore may still
	 * block), false otherwise.
	 */
	bool may_block();

	/**
	 * If this is a relative timeout, elapse the specified number of ticks.
	 * Otherwise, do nothing.
	 */
	void elapse(Ticks ticks);

	/**
	 * Take any ticks that have been counted towards `source` and apply it to
	 * `destination`.  If `destination` is not a tick-counting relative timeout,
	 * do nothing.
	 *
	 * This is intended for short-yielding operations, where a compartment
	 * wishes to yield for a small amount of time (typically one tick) and then
	 * retry some operation, rather than waiting for the whole time that the
	 * caller has allowed them to wait.  The caller-provided timeout may be
	 * relative or absolute, but the timeout for the short sleep is local to the
	 * compartment doing the short sleep and so is always a `Timeout`.
	 */
	void elapse_from(Timeout &source)
	{
		elapse(source.elapsed);
	}
#endif
} TimeoutArgument;

_Static_assert(sizeof(TimeoutArgument) == sizeof(void *),
               "Timeout arguments should be passable in a single register");

/**
 * Check that the argument is a valid pointer to a `Timeout` structure.  This
 * must have read/write permissions, be unsealed, and must not be a heap
 * address.
 *
 * This function is not recommended for new code.  The more general
 * `timeout_is_valid` call checks that this is a valid timeout argument.
 */
bool __cheri_libcall check_timeout_pointer(const struct Timeout *timeout);

/**
 * Helper to determine whether a timeout's `relativeTimeout` alternative is the
 * active one.
 */
__if_c(static) inline bool timeout_is_relative(TimeoutArgument timeout)
{
	return cheri_is_valid(timeout.relativeTimeout);
}

/**
 * Returns true if this is a valid `TimeoutArgument`: either an untagged value
 * or a valid pointer to a `Timeout`.
 */
bool __cheriot_libcall timeout_is_valid(TimeoutArgument timeout);

/**
 * Returns true if the timeout is in the past (for relative timeouts, if there
 * is no remaining time), false otherwise.
 */
bool __cheriot_libcall timeout_has_expired(TimeoutArgument timeout);

/**
 * Returns true if the timeout is in the future (and therefore may still
 * block), false otherwise.
 */
__if_c(static) inline bool timeout_may_block(TimeoutArgument timeout)
{
	return !timeout_has_expired(timeout);
}

/**
 * If this is a relative timeout, elapse the specified number of ticks.
 * Otherwise, do nothing.
 */
void __cheriot_libcall timeout_elapse(TimeoutArgument timeout, Ticks ticks);

/**
 * Take any ticks that have been counted towards `source` and apply it to
 * `destination`.  If `destination` is not a tick-counting relative timeout, do
 * nothing.
 *
 * This is intended for short-yielding operations, where a compartment wishes
 * to yield for a small amount of time (typically one tick) and then retry some
 * operation, rather than waiting for the whole time that the caller has
 * allowed them to wait.  The caller-provided timeout may be relative or
 * absolute, but the timeout for the short sleep is local to the compartment
 * doing the short sleep and so is always a `Timeout`.
 */
__if_c(static) inline void timeout_elapse_from(TimeoutArgument destination,
                                               Timeout        *source)
{
	timeout_elapse(destination, source->elapsed);
}

#ifdef __cplusplus
inline bool TimeoutArgument::is_valid()
{
	return timeout_is_valid(*this);
}
inline bool TimeoutArgument::may_block()
{
	return timeout_may_block(*this);
}
inline void TimeoutArgument::elapse(Ticks ticks)
{
	return timeout_elapse(*this, ticks);
}
#endif

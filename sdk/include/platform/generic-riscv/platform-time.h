// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
#pragma once

#include <cdefs.h>
#include <stdint.h>

/**
 * @file `platform-time.h` defines the interface for the timer that is used to
 * implement a monotonic clock.  This is the generic implementation that
 * assumes that the time value is exposed via a CSR.  If your board exposes
 * this via an MMIO region then it must provide a different implementation.
 */

/**
 * `CHERIOT_PLATFORM_TIME_HIGH` is a macro that can be defined by platform
 * headers that include this via `#include_next`.  This defines the name for
 * the CSR that provides the top 32 bits of a 64-bit counter.
 */
#ifndef CHERIOT_PLATFORM_TIME_HIGH
#	define CHERIOT_PLATFORM_TIME_HIGH mcycleh
#endif

/**
 * `CHERIOT_PLATFORM_TIME_LOW` is a macro that can be defined by platform
 * headers that include this via `#include_next`.  This defines the name for
 * the CSR that provides the top 32 bits of a 64-bit counter.
 */
#ifndef CHERIOT_PLATFORM_TIME_LOW
#	define CHERIOT_PLATFORM_TIME_LOW mcycle
#endif

/**
 * Platform-specific hook for reading the current monotonic time.  This should
 * be implemented to return the time that increments with the rate defined by
 * the `CPU_TIMER_HZ` macro and must be the same timer used by the scheduler.
 */
__if_c(static) inline uint64_t platform_monotonic_time_read()
{
	uint32_t mtimehBefore;
	uint32_t mtime;
	uint32_t mtimehAfter;
	/* Read the two 32-bit time CSRs.  Read the high value first, and then check
	 * whether it has changed.  This can happen in two situations:
	 *
	 * If we are reading near the point where the low value overflows.  This
	 * would cause the read of high then low to appear almost 2^32 ticks in the
	 * past.
	 *
	 * If we are preempted in the middle of the read, it's possible that the
	 * value of the high counter may be incremented (possibly by more than one)
	 * and the value of the low would change an arbitrary amount.
	 *
	 * Note that, if not called with interrupts disabled, it's still possible
	 * that we are preempted immediately *after* this returns, so the time read
	 * here may be an arbitrary time in the past, but it is guaranteed to be a
	 * point between when this function is entered and when it returns.
	 */
	do
	{
		__asm__ volatile("csrr %0, " __XSTRING(CHERIOT_PLATFORM_TIME_HIGH)
		                 : "=r"(mtimehBefore));
		__asm__ volatile("csrr %0, " __XSTRING(CHERIOT_PLATFORM_TIME_LOW)
		                 : "=r"(mtime));
		__asm__ volatile("csrr %0, " __XSTRING(CHERIOT_PLATFORM_TIME_HIGH)
		                 : "=r"(mtimehAfter));
	} while (mtimehBefore != mtimehAfter);
	// This header is included from C and C++, don't do C++-specific lints in
	// it. NOLINTNEXTLINE(google-readability-casting)
	return ((uint64_t)(mtimehAfter) << 32) + mtime;
}

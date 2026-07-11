// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
#pragma once

/**
 * \file .
 *
 * Standard clock support.
 */

#include <platform-time.h>
#include <stdint.h>
#include <sys/time.h>
#include <thread.h>

// The names in this file come from C or POSIX and so do not correspond to our
// naming scheme.  This header is expected to be included in C, so should also
// not provide warnings about void in function parameter lists.
// NOLINTBEGIN(readability-identifier-naming,modernize-redundant-void-arg)

/// ID for a clock.  Only monotonic and 'realtime' (wall clock) are supported.
typedef enum __clockid_t
{
	/**
	 * The monotonic clock. This is zero at system start and increments at a
	 * fixed rate.
	 */
	CLOCK_MONOTONIC,
	/**
	 * The wall-clock time.
	 *
	 * This clock's value is meaningful only if `clock_update_wall_clock` has
	 * been called at least once and there is at least one working clock source
	 * in the system.
	 */
	CLOCK_REALTIME,
	/**
	 * CPU time consumed by the current thread, since boot time.  This clock is
	 * equivalent to `CLOCK_MONOTONIC` if the scheduler is not compiled with
	 * support for accounting.  Add `--scheduler-accounting=y` to your build
	 * configuration line to enable this.
	 */
	CLOCK_THREAD_CPUTIME_ID,
	/**
	 * CPU time consumed by the current 'process'.  This value is defined by
	 * POSIX, but CHERIoT RTOS does not have a direct equivalent of a process
	 * and so this value is equivalent to `CLOCK_THREAD_CPUTIME_ID`, with all of
	 * the attendant caveats.
	 */
	CLOCK_PROCESS_CPUTIME_ID = CLOCK_THREAD_CPUTIME_ID,
} clockid_t;

/**
 * The number of ticks on the monotonic clock per second.
 */
#define CLOCKS_PER_SEC ((clock_t)CPU_TIMER_HZ)

/**
 * Flag to indicate that a timespec should be treated as an absolute, rather
 * than relative, time.
 */
#define TIMER_ABSTIME 1

/**
 * Type for holding time.  The `CLOCKS_PER_SEC` macro defines the value in this
 * type that corresponds to one second.  This rate is SoC-specific.
 */
typedef uint64_t clock_t;

/**
 * A time value with up to nanosecond precision.
 */
struct timespec
{
	/// Seconds
	time_t tv_sec;
	/// Nanoseconds
	long tv_nsec;
};

__BEGIN_DECLS

/**
 * Returns the amount of CPU time (in units defined by `CLOCKS_PER_SEC`) that
 * are accounted to the current thread (POSIX specifies 'process' here, but
 * CHERIoT RTOS does not have an directly analogous abstraction).
 *
 * Note: If scheduler accounting is not enabled, this API will return the
 * total elapsed uptime instead.  Add `--scheduler-accounting=y` to your build
 * configuration line to enable this.
 */
static inline clock_t clock(void)
{
#if SCHEDULER_ACCOUNTING == true
	return thread_elapsed_cycles_current();
#else
	return platform_monotonic_time_read();
#endif
}

/**
 * Retrieve the time from the specified clock as a `timespec`.
 *
 * If `clockID` is `CLOCK_REALTIME`, the returned value is meaningful only if
 * `clock_update_wall_clock` has been called at least once and there is at least
 * one working clock source in the system.
 */
__cheriot_libcall int clock_gettime(clockid_t        clockID,
                                    struct timespec *outTime);

/**
 * Update the wall-clock time from available time sources.
 */
__cheriot_compartment("wall_clock") int clock_update_wall_clock(
  TimeoutArgument timeout);

/**
 * POSIX-compatible time() implementation.  Returns the time in seconds since
 * the UNIX epoch.
 *
 * This value is meaningful only if `clock_update_wall_clock` has been called at
 * least once and there is at least one working clock source in the system.
 *
 */
__cheriot_libcall time_t time(time_t *tloc);

__END_DECLS

// NOLINTEND(readability-identifier-naming,modernize-redundant-void-arg)

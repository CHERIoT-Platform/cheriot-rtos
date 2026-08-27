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
	/**
	 * Nanoseconds
	 *
	 * Note that POSIX and pre-C23 versions of C specify that this is `long`,
	 * but they require that the values be between 0 and 999,999,999
	 * (inclusive).  C23 allows this to be any type capable of representing
	 * this range and so we use `uint32_t`.
	 */
	uint32_t tv_nsec;
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

/**
 * Structure representing a date in the Gregorian calendar.
 *
 * This is intended to be compatible with C/POSIX, not all fields are used by
 * all APIs.
 */
struct tm
{
	/// Seconds, in the range 0--60 (to account for leap seconds).
	int tm_sec;
	/// Minutes, in the range 0--59.
	int tm_min;
	/// Hours, in the range 0--23.
	int tm_hour;
	/**
	 * Day of the month, in the range 1--31, or less if the month has fewer
	 * than 31 days.
	 */
	int tm_mday;
	/**
	 * Month of the year, in the range 0--11.  Note that this counts from 0,
	 * whereas days of the month count from 1.
	 */
	int tm_mon;
	/// Year, as an offset from 1900 (so, for example, 2023 is 123).
	int tm_year;
	/**
	 * Day of the week, in the range 0--6.  Sunday is 0, Saturday is 6.
	 */
	int tm_wday;
	/**
	 * Day of the year, in the range 0--365 (0--364 if this is not a leap year).
	 */
	int tm_yday;
	/**
	 * Daylight savings flag.
	 */
	int tm_isdst;
};

/**
 * Convert a `struct tm` to a `time_t`.  This is intended to be compatible with
 * the BSD extension and is equivalent to the POSIX `mktime` with a UTC locale.
 *
 * The `tm_wday` and `tm_yday` fields are ignored as inputs.  Other fields may
 * be out of range, for example an hour of -1 means hour 22 in the previous day,
 * a day of 40 in a month with 31 days means day 9 in the next month, and so on.
 *
 * The values of the `tm_wday` and `tm_yday` fields will be set on successful
 * completion.
 *
 * NOTE: UNIX time stamps do not include leap seconds.  If a leap second (the
 * 60th second at the end of June or December in a year that contains one) is
 * specified in `time`, it will be treated as an overflow and the result of
 * this function will be off by one.
 */
time_t __cheriot_libcall timegm(struct tm *time);

/**
 * C standard function to calculate a human-readable UTC date and time in a
 * `struct tm` from a UNIX timestamp passed indirectly as `timer`.  The
 * `result` argument is used to provide space for the output.  The return value
 * is `result`, or an untagged value if an error occurs.
 */
struct tm *__cheriot_libcall gmtime_r(const time_t *__restrict timer,
                                      struct tm *__restrict result);

/**
 * C standard function to calculate a human-readable UTC date and time in a
 * `struct tm` from a UNIX timestamp.  This uses an internal buffer that is
 * invalidated on each subsequent call and is not thread safe.  `gmtime_r`
 * should be used instead.
 */
static inline struct tm *gmtime(const time_t *timer)
{
	static struct tm result;
	return gmtime_r(timer, &result);
}

__END_DECLS

// NOLINTEND(readability-identifier-naming,modernize-redundant-void-arg)

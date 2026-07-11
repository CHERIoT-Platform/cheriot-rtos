// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "wall_clock.hh"
#include <compartment-macros.h>
#include <debug.hh>
#include <sys/time.h>
#include <tick_macros.h>
#include <time.h>

using Debug = ConditionalDebug<false, "Clock helper">;

namespace
{
	/**
	 * Internal helper that fetches the current time from the shared-memory
	 * object.
	 */
	clock_t wall_clock_time_get()
	{
		auto &synchronisedTime = *SHARED_OBJECT_WITH_PERMISSIONS(
		  SynchronisedTime, wall_clock_time, true, false, false, false, false);
		clock_t  wallClockTime;
		clock_t  monotonicTime;
		uint32_t epoch;
		do
		{
			epoch = synchronisedTime.updatingEpoch.load();
			// If the low bit is set then the time is being updated.  Wait for
			// the update to finish.
			if (epoch & 0x1)
			{
				Debug::log("Waiting for published time to update");
				// Wait for the update to finish, include a timeout in case the
				// update doesn't have enough stack or trusted stack to notify
				// the futex.
				Timeout t{MS_TO_TICKS(10)};
				synchronisedTime.updatingEpoch.wait(&t, epoch);
				continue;
			}
			wallClockTime = synchronisedTime.wallClockTime;
			monotonicTime = synchronisedTime.monotonicTime;
		} while (epoch != synchronisedTime.updatingEpoch.load());
		// Fetch the monotonic time to calculate the time elapsed since the
		// snapshot was published.
		uint64_t now = platform_monotonic_time_read();
		// Elapsed time in cycles
		uint64_t elapsed = now - monotonicTime;
		Debug::log(
		  "Elapsed cycles {} (elapsed seconds: {}) (now: {}, timestamp: {})",
		  static_cast<int64_t>(elapsed),
		  static_cast<int>(elapsed / CPU_TIMER_HZ),
		  static_cast<int64_t>(now),
		  static_cast<int64_t>(monotonicTime));
		return wallClockTime + elapsed;
	}
} // namespace

int gettimeofday(struct timeval *tp, void *)
{
	clock_t now        = wall_clock_time_get();
	tp->tv_sec         = now / CPU_TIMER_HZ;
	clock_t subseconds = now % CPU_TIMER_HZ;
	// Avoid integer rounding errors by *first* multiplying and then dividing.
	// This is equivalent to subseconds /= (CPU_TIMER_HZ / 1'000'000).
	subseconds *= 1'000'000;
	subseconds /= CPU_TIMER_HZ;
	tp->tv_usec = subseconds;
	return 0;
}

int clock_gettime(clockid_t clockID, struct timespec *tp)
{
	clock_t now = 0;
	switch (clockID)
	{
		case CLOCK_THREAD_CPUTIME_ID:
			// Fall back to monotonic time if scheduler accounting is disabled.
#if SCHEDULER_ACCOUNTING == true
			now = thread_elapsed_cycles_current();
			break;
#else
			[[fallthrough]];
#endif
		case CLOCK_MONOTONIC:
			now = platform_monotonic_time_read();
			break;
		case CLOCK_REALTIME:
			now = wall_clock_time_get();
			break;
		default:
			return -1;
	}
	tp->tv_sec         = now / CPU_TIMER_HZ;
	clock_t subseconds = now % CPU_TIMER_HZ;
	// Avoid integer rounding errors by *first* multiplying and then dividing.
	// This is equivalent to subseconds /= (CPU_TIMER_HZ / 1'000'000'000).  Note
	// that tv_usec is a 32-bit value (nanoseconds in
	subseconds *= 1'000'000'000;
	subseconds /= CPU_TIMER_HZ;
	tp->tv_nsec = static_cast<long>(subseconds);
	return 0;
}

time_t time(time_t *tloc)
{
	clock_t now        = wall_clock_time_get();
	time_t  nowSeconds = now / CPU_TIMER_HZ;
	if (tloc != NULL)
	{
		*tloc = nowSeconds;
	}
	return nowSeconds;
}

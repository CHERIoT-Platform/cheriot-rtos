#include "lock_debug.hh"
#include <errno.h>
#include <locks.h>

int barrier_timed_wait(Timeout *timeout, struct BarrierState *barrier)
{
	auto remaining = --barrier->remaining;
	Debug::Assert(remaining < std::numeric_limits<int32_t>::max(),
	              "Either you have an implausible number of threads, or this "
	              "barrier has underflowed with {} waiters",
	              remaining);

	// If we're the last thread to reach this barrier, wake up everyone else
	// and return 0.
	if (remaining == 0)
	{
		Debug::log("Last thread arrived, waking barrier {}", barrier);
		barrier->remaining.notify_all();
		return 0;
	}

	// If there are still waiters, we're going to sleep.
	while (remaining != 0)
	{
		Debug::log("No the last thread, sleeping on barrier {}", barrier);
		int result = barrier->remaining.wait(timeout, remaining);
		// If the sleep timed out, let the caller know.
		if (result == -ETIMEDOUT)
		{
			Debug::log("Timed out sleeping on barrier {}", barrier);
			return result;
		}
		// Read the atomic again and loop.
		remaining = barrier->remaining;
	}
	Debug::log("Woke after sleeping on barrier {}", barrier);

	// Return indicating that we were one of the blocked threads.
	return 1;
}

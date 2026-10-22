#include <errno.h>
#include <futex.h>
#include <thread.h>

int thread_await_poll(TimeoutArgument timeout,
                      uint16_t        maximumYields,
                      int (*callback)(void *),
                      void *argument)
{
	int res;

	// Poll with yields without acruing time
	for (uint16_t i = 0; i < maximumYields; i++)
	{
		if ((res = callback(argument)) != 0)
		{
			return res;
		}

		yield();
	}

	if ((res = callback(argument)) != 0)
	{
		return res;
	}

	while (timeout.may_block())
	{
		/*
		 * The analogous behavior in interrupt_poll_wait is that we'd allow
		 * lower priority work to progress, because futex_timed_wait blocks us.
		 * thread_sleep() would ordinarily come right back to us if there were
		 * no other equal priority threads, but using `ThreadSleepNoEarlyWake`
		 * will force `thread_sleep` to block us for the one tick and cede the
		 * core to lower priority runnable threads, if any.
		 */

		Timeout t{1};
		if ((res = thread_sleep(&t, ThreadSleepNoEarlyWake)) != 0)
		{
			return res;
		}
		timeout.elapse_from(t);

		if ((res = callback(argument)) != 0)
		{
			return res;
		}
	}

	return -ETIMEDOUT;
}

int futex_await_poll(TimeoutArgument          timeout,
                     uint16_t                 maximumYields,
                     const volatile uint32_t *futex,
                     void (*enableCallback)(void *),
                     int (*checkCallback)(void *),
                     void *argument)
{
	int res;

	// Poll with yields without acruing time
	for (uint16_t i = 0; i < maximumYields; i++)
	{
		if ((res = checkCallback(argument)) != 0)
		{
			return res;
		}

		yield();
	}

	if ((res = checkCallback(argument)) != 0)
	{
		return res;
	}

	enableCallback(argument);

	/*
	 * The interrupt that signals our `checkCallback` changing value may fire
	 * at any point between the last call thereto and the next call thereto, in
	 * the loop below.  It may even fire before we read the futex value.  This
	 * is fine: the futex read is there to catch future edges if the next call
	 * returns 0, which won't happen if there's already been an edge before that
	 * call.
	 */

	uint32_t lastFutexValue;

	do
	{
		// Prevent prior writes from reordering after the futex read
		__c11_atomic_signal_fence(__ATOMIC_RELEASE);
		lastFutexValue = *futex;
		// Prevent subsequent loads from reordering prior to the futex read
		__c11_atomic_signal_fence(__ATOMIC_ACQUIRE);

		if ((res = checkCallback(argument)) != 0)
		{
			return res;
		}
	} while ((res = futex_timed_wait(timeout, futex, lastFutexValue)) == 0);

	return res;
}

#include <errno.h>
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

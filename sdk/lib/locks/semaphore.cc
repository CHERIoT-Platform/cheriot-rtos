#include <errno.h>
#include <locks.h>

namespace
{
	constexpr uint32_t WaitersBit = 1 << 31;
} // namespace

int semaphore_get(Timeout *timeout, CountingSemaphoreState *semaphore)
{
	do
	{
		uint32_t value      = semaphore->count.load();
		uint32_t count      = value & ~WaitersBit;
		bool     hasWaiters = value & WaitersBit;
		// If the count is greater than zero, we should be able to just acquire
		// a semaphore.
		if (count > 0)
		{
			if (semaphore->count.compare_exchange_strong(value, value - 1))
			{
				return 0;
			}
			continue;
		}
		// If there are no waiters, mark this as adding one.
		if (!hasWaiters)
		{
			// If we lost a race, retry.
			if (!semaphore->count.compare_exchange_strong(value,
			                                              value | WaitersBit))
			{
				continue;
			}
		}
		// If we fail in the futex wait, return the error.
		if (int ret = semaphore->count.wait(timeout, value | WaitersBit);
		    ret != 0)
		{
			return ret;
		}
	} while (true);
}

int semaphore_put(CountingSemaphoreState *semaphore)
{
	do
	{
		uint32_t value      = semaphore->count.load();
		uint32_t count      = value & ~WaitersBit;
		bool     hasWaiters = value & WaitersBit;
		if (count == semaphore->maxCount)
		{
			return -EINVAL;
		}
		if (semaphore->count.compare_exchange_strong(value, count + 1))
		{
			// If there were waiters, wake them.
			if (hasWaiters)
			{
				semaphore->count.notify_all();
			}
			return 0;
		}
	} while (true);
}

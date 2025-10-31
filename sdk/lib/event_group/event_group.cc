#include <atomic>
#include <cstdlib>
#include <errno.h>
#include <event.h>
#include <locks.hh>
#include <stdint.h>
#include <thread.h>

using Debug = ConditionalDebug<false, "Event groups library">;

struct EventWaiter
{
	std::atomic<uint32_t> bitsSeen;
	bool                  waitForAll : 1;
	bool                  clearOnExit : 1;
	unsigned int          bitsWanted : 24;
	bool                  is_triggered(uint32_t bits)
	{
		Debug::log("bits wanted: {}, bits: {}, mask: {}",
		           bitsWanted,
		           bits,
		           (bitsWanted & bits));
		return (waitForAll ? ((bitsWanted & bits) == bitsWanted)
		                   : ((bitsWanted & bits) != 0));
	};
};

struct EventGroup
{
	FlagLock    lock;
	uint32_t    bits;
	size_t      waiterCount;
	EventWaiter waiters[];
};

int eventgroup_create(Timeout            *timeout,
                      AllocatorCapability heapCapability,
                      EventGroup        **outGroup)
{
	auto threads = thread_count();
	if (threads == static_cast<uint16_t>(-1))
	{
		return -ERANGE;
	}
	size_t size = sizeof(EventGroup) + (threads * sizeof(EventWaiter));
	auto   group =
	  static_cast<EventGroup *>(heap_allocate(timeout, heapCapability, size));
	*outGroup = group;
	if (!__builtin_cheri_tag_get(group))
	{
		return -ENOMEM;
	}
	group->waiterCount = threads;
	return 0;
}

int eventgroup_wait(Timeout    *timeout,
                    EventGroup *group,
                    uint32_t   *outBits,
                    uint32_t    bitsWanted,
                    bool        waitForAll,
                    bool        clearOnExit)
{
	// Bits wanted can be only a 24-bit value.
	if (bitsWanted & 0xff000000)
	{
		return -ERANGE;
	}
	// Condition that holds if the bits are triggered.
	auto isTriggered = [&](uint32_t bits) {
		return (waitForAll ? ((bitsWanted & bits) == bitsWanted)
		                   : ((bitsWanted & bits) != 0));
	};
	auto    &waiter = group->waiters[thread_id_get() - 1];
	uint32_t bitsSeen;
	// Set up our state for the waiter with the lock held.
	if (LockGuard g{group->lock, timeout})
	{
		bitsSeen = group->bits;
		// If the condition holds, return immediately
		if (isTriggered(bitsSeen))
		{
			if (clearOnExit)
			{
				group->bits &= ~bitsWanted;
			}
			*outBits = bitsSeen;
			return 0;
		}
		waiter.bitsWanted  = bitsWanted;
		waiter.clearOnExit = clearOnExit;
		waiter.waitForAll  = waitForAll;
		waiter.bitsSeen    = bitsSeen;
	}
	else
	{
		return -ETIMEDOUT;
	}
	// If the condition didn't hold, wait for
	Debug::log("Waiting on futex {} ({})", &waiter.bitsSeen, bitsSeen);
	while (waiter.bitsSeen.wait(timeout, bitsSeen) != -ETIMEDOUT)
	{
		bitsSeen = waiter.bitsSeen.load();
		if (isTriggered(bitsSeen))
		{
			*outBits = bitsSeen;
			return 0;
		}
	};
	waiter.bitsWanted = 0;
	*outBits          = group->bits;
	return -ETIMEDOUT;
}

int eventgroup_clear(Timeout    *timeout,
                     EventGroup *group,
                     uint32_t   *outBits,
                     uint32_t    bitsToClear)
{
	if (LockGuard g{group->lock, timeout})
	{
		Debug::log(
		  "Bits was {}, clearing with mask {}", group->bits, ~bitsToClear);
		group->bits &= ~bitsToClear;
		*outBits = group->bits;
		return 0;
	}
	*outBits = group->bits;
	return -ETIMEDOUT;
}

int eventgroup_set(Timeout    *timeout,
                   EventGroup *group,
                   uint32_t   *outBits,
                   uint32_t    bitsToSet)
{
	if (LockGuard g{group->lock, timeout})
	{
		Debug::log("Bits was {}, setting {}", group->bits, bitsToSet);
		group->bits |= bitsToSet;
		uint32_t bits        = group->bits;
		uint32_t bitsToClear = 0;
		Debug::log("Bits {} are set", bits);
		for (size_t i = 0; i < group->waiterCount; ++i)
		{
			auto &waiter = group->waiters[i];
			Debug::log("Waiter {} wants bits {}", i, waiter.bitsWanted);
			if (waiter.bitsWanted == 0)
			{
				continue;
			}
			Debug::log("Triggered? {}", waiter.is_triggered(bitsToSet));
			if (waiter.is_triggered(bits))
			{
				if (waiter.clearOnExit)
				{
					bitsToClear |= (waiter.bitsWanted & bits);
				}
				waiter.bitsWanted = 0;
				waiter.bitsSeen   = bits;
				Debug::log("Waking futex {} ({})", &waiter.bitsSeen, bits);
				waiter.bitsSeen.notify_one();
			}
		}
		Debug::log("Clearing bits {}", bitsToClear);
		group->bits &= ~bitsToClear;
		*outBits = group->bits;
		return 0;
	}
	*outBits = group->bits;
	return -ETIMEDOUT;
}

int eventgroup_get(EventGroup *group, uint32_t *outBits)
{
	*outBits = group->bits;
	return 0;
}

int eventgroup_destroy_force(AllocatorCapability heapCapability,
                             EventGroup         *group)
{
	group->lock.upgrade_for_destruction();
	// Force all waiters to wake.
	for (size_t i = 0; i < group->waiterCount; ++i)
	{
		// Notifying is not enough, we need to ensure that waiters get
		// appropriate bits to leave the loop.
		auto    &waiter   = group->waiters[i];
		uint32_t bits     = waiter.bitsWanted;
		waiter.bitsWanted = 0;
		waiter.bitsSeen   = bits;
		waiter.bitsSeen.notify_one();
	}
	return heap_free(heapCapability, group);
}

int eventgroup_destroy(AllocatorCapability heapCapability, EventGroup *group)
{
	group->lock.lock();
	return eventgroup_destroy_force(heapCapability, group);
}

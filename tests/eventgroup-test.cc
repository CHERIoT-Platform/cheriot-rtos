#include "thread.h"
#include <cstdint>
#include <cstdlib>
#define TEST_NAME "Event Group"
#include "tests.hh"
#include <atomic>
#include <event.h>
#include <thread_pool.h>

using thread_pool::async;

namespace
{
	std::atomic<uint32_t> counter{2};
	void                  barrier()
	{
		int c = --counter;
		debug_log("Thread {} waiting for barrier {}", thread_id_get(), c);
		while (counter > 0)
		{
			counter.wait(c);
			c = counter.load();
		}
		counter.notify_all();
	}
} // namespace

int test_eventgroup()
{
	EventGroup *group;

	Timeout t{1};
	int     ret = eventgroup_create(&t, MALLOC_CAPABILITY, &group);
	TEST(ret == 0, "Failed to create event group: {}", ret);

	t = 0;
	uint32_t bits;
	ret = eventgroup_set(&t, group, &bits, 1);
	TEST(ret == 0, "Failed to set event group bits: {}", ret);
	TEST(bits == 1, "Bits should be 1, but is {}", bits);
	eventgroup_get(group, &bits);
	TEST(bits == 1, "Fetched bits ({}) not the same as set bits (1)", bits);

	async([=]() {
		Timeout  t{2};
		uint32_t bits;
		int      ret = eventgroup_set(&t, group, &bits, 0b10);
		debug_log("eventgroup_set returned {}", ret);
		barrier();
		TEST(ret == 0, "Failed to set event group bits: {}", ret);
		TEST(bits == 0,
		     "Bits should be 0 (all cleared by clearOnExit), but is {}",
		     bits);
	});

	t   = 4;
	ret = eventgroup_wait(&t, group, &bits, 0b11, true, true);
	debug_log("eventgroup_wait returned {}", ret);
	TEST(ret == 0, "Failed to wait for event group: {}", ret);
	barrier();
	TEST(bits == 0b11, "Bits should be 0b11, but is {}", bits);
	eventgroup_get(group, &bits);
	TEST(bits == 0,
	     "Fetched bits {} after clearOnExit should have set them to 0",
	     bits);

	ret = eventgroup_set(&t, group, &bits, 0b1100);
	TEST(ret == 0, "Failed to set bits for event group: {}", ret);
	TEST(bits == 0b1100, "Bits should be 0b1100, but is {}", bits);
	ret = eventgroup_clear(&t, group, &bits, 0b100);
	TEST(ret == 0, "Failed to clear event group bits: {}", ret);
	TEST(bits == 0b1000, "Bits should be 0b1000, but is {}", bits);
	return 0;
}

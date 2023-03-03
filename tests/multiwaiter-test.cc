// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Multiwaiter"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>
#include <event.h>
#include <futex.h>
#include <multiwaiter.h>
#include <queue.h>
#include <thread.h>
#include <thread_pool.h>

using namespace CHERI;
using namespace thread_pool;

void test_multiwaiter()
{
	static uint32_t futex  = 0;
	static uint32_t futex2 = 0;
	int             ret;
	MultiWaiter    *mw;
	Timeout         t{0};
	ret = multiwaiter_create(&t, MALLOC_CAPABILITY, &mw, 4);
	TEST((ret == 0) && (mw != nullptr),
	     "Allocating multiwaiter failed {} ({})",
	     ret,
	     mw);
	debug_log("Allocated multiwaiter {}");

	t.remaining = 5;
	EventWaiterSource events[4];

	debug_log("Testing error case: Invalid values");
	events[0] = {nullptr, static_cast<EventWaiterKind>(5), 0};
	ret       = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == -EINVAL, "multiwaiter returned {}, expected {}", ret, -EINVAL);

	debug_log("Testing one futex, already ready");
	events[0]   = {&futex, EventWaiterFutex, 1};
	t.remaining = 5;
	ret         = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "multiwaiter returned {}, expected 0", ret);

	auto setFutex = [](uint32_t *futexWord, uint32_t value) {
		async([=]() {
			sleep(1);
			debug_log("Waking futex from background thread");
			*futexWord = value;
			futex_wake(futexWord, 1);
		});
	};

	debug_log("Testing one futex, not yet ready");
	setFutex(&futex, 1);
	events[0]   = {&futex, EventWaiterFutex, 0};
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "multiwaiter returned {}, expected 0", ret);

	debug_log("Testing two futexes, not yet ready");
	futex  = 0;
	futex2 = 2;
	setFutex(&futex2, 3);
	events[0]   = {&futex, EventWaiterFutex, 0};
	events[1]   = {&futex2, EventWaiterFutex, 2};
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 2);
	TEST(ret == 0, "multiwaiter returned {}, expected 0", ret);
	TEST(events[0].value == 0, "Futex reports wake but none occurred");
	TEST(events[1].value == 1, "Futex reports no wake");

	void *queue;
	t.remaining = 0;
	ret         = queue_create(&t, MALLOC_CAPABILITY, &queue, sizeof(int), 1);
	TEST(ret == 0, "Queue create failed:", ret);
	int     val = 0;
	Timeout noWait{0};
	ret = queue_send(&noWait, queue, &val);
	TEST(ret == 0, "Queue send failed: {}", ret);

	debug_log("Testing queue, blocked on send");
	async([=]() {
		sleep(1);
		int     val;
		Timeout noWait{0};
		int     ret = queue_recv(&noWait, queue, &val);
		TEST(ret == 0, "Background receive failed: {}", ret);
		TEST(val == 0, "Background receive returned incorrect value: {}", ret);
		debug_log("Background thread made queue ready to send");
	});
	events[0]   = {queue, EventWaiterQueue, EventWaiterQueueSendReady};
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "multiwaiter returned {}, expected 0", ret);
	TEST(events[0].value == EventWaiterQueueSendReady,
	     "Queue reports not ready");

	debug_log("Testing queue, blocked on receive");
	async([=]() {
		sleep(1);
		int     val = 1;
		Timeout noWait{0};
		int     ret = queue_send(&noWait, queue, &val);
		TEST(ret == 0, "Background send failed: {}", ret);
		debug_log("Background thread made queue ready to receive");
	});
	events[0]   = {queue, EventWaiterQueue, EventWaiterQueueReceiveReady};
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "multiwaiter returned {}, expected 0", ret);
	TEST(events[0].value == EventWaiterQueueReceiveReady,
	     "Queue did not return ready to receive");
	ret = queue_recv(&noWait, queue, &val);
	TEST(ret == 0, "Queue ready to receive but receive returned {}", ret);
	TEST(val == 1, "Incorrect value returned from queue");

	debug_log("Testing waiting on a queue and a futex");
	futex = 0;
	setFutex(&futex, 1);
	events[0]   = {queue, EventWaiterQueue, EventWaiterQueueReceiveReady};
	events[1]   = {&futex, EventWaiterFutex, 0};
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 2);
	TEST(ret == 0, "multiwait on futex and queue returned {}", ret);
	TEST(events[0].value == 0,
	     "Queue reports ready to receive but should be empty.");
	TEST(events[1].value == 1, "Futex reports no wake");

	void *ev;
	ret = event_create(&noWait, MALLOC_CAPABILITY, &ev);
	TEST(ret == 0, "Failed to create event channel");

	debug_log("Testing event channel wait that shouldn't trigger wake");
	auto setBits = [=](uint32_t newBits) {
		uint32_t bits;
		event_bits_set(ev, &bits, newBits);
		return bits;
	};
	async([=]() {
		sleep(1);
		setBits(0b1010);
		debug_log("Set some bits in the background");
		sleep(1);
		futex = 1;
		futex_wake(&futex, 1);
	});
	events[0] = {ev, EventWaiterEventChannel, 0b101};
	futex     = 0;
	events[1] = {&futex, EventWaiterFutex, 0};
	// This should not return until the futex fires...
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 2);
	TEST(ret == 0, "Wait for event channel and futex failed: {}", ret);
	TEST(events[0].value == 0,
	     "Event channel returned wrong bits: {}",
	     events[0].value);
	TEST(
	  events[1].value == 1, "Futex returned {}, expected 1", events[1].value);
	uint32_t bits;
	event_bits_clear(ev, &bits, 0b1111);

	debug_log("Testing event channel wait for all.");
	async([=]() {
		sleep(1);
		setBits(0b1010);
		debug_log("Set some bits in the background");
		sleep(1);
		setBits(0b101);
	});
	events[0] = {
	  ev, EventWaiterEventChannel, EventWaiterEventChannelWaitAll | 0b1111};
	// This should not return until the second set bits.
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "Wait for event channel failed: {}", ret);
	TEST(events[0].value == 0b1111,
	     "Event channel returned wrong bits: {}",
	     events[0].value);
	event_bits_clear(ev, &bits, 0b1111);

	debug_log("Testing event channel wait and clear.");
	async([=]() {
		sleep(1);
		setBits(0b1);
		debug_log("Set some bits in the background");
	});
	// Bit 24 is autoclear.
	events[0] = {
	  ev, EventWaiterEventChannel, EventWaiterEventChannelClearOnExit | 0b1};
	// This should not return until the bottom bit is set.
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "Wait for event channel failed: {}", ret);
	TEST(events[0].value == 0b1,
	     "Event channel returned wrong bits: {}",
	     events[0].value);
	uint32_t testBits;
	ret = event_bits_get(ev, &testBits);
	TEST(ret == 0, "Fetching event bits failed: {}", ret);
	TEST(testBits == 0,
	     "Event channel bits should have been cleared, but got {}",
	     testBits);

	debug_log("Testing event channel wait for some");
	async([=]() {
		sleep(1);
		auto v = setBits(0b1010);
		debug_log("Set some bits in the background ({})", v);
		sleep(5);
		debug_log("Set some more bits in the background");
		setBits(0b101);
	});
	events[0] = {ev, EventWaiterEventChannel, 0b1111};
	// We should be woken up after the first 0b1010 setBits call.
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "Wait for event channel failed: {}", ret);
	TEST(events[0].value == 0b1010,
	     "Event channel returned wrong bits: {}, expected {}",
	     events[0].value,
	     0b1010U);
	// Make sure we don't signal the event channel after it's been deallocated
	// by waiting until the async has completed.
	events[0].value = EventWaiterEventChannelWaitAll | 0b1111;
	t.remaining     = 10;
	multiwaiter_wait(&t, mw, events, 1);

	event_delete(MALLOC_CAPABILITY, ev);
	queue_delete(MALLOC_CAPABILITY, queue);
	multiwaiter_delete(MALLOC_CAPABILITY, mw);
}

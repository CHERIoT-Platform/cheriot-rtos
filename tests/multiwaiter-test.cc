// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Multiwaiter"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>
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
	debug_log("Allocated multiwaiter {}", mw);

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

	QueueHandle queue;
	void       *queueMemory;
	t.remaining = 0;
	ret =
	  queue_create(&t, MALLOC_CAPABILITY, &queue, &queueMemory, sizeof(int), 1);

	TEST(ret == 0, "Queue create failed:", ret);
	int     val = 0;
	Timeout noWait{0};
	ret = queue_send(&noWait, &queue, &val);
	TEST(ret == 0, "Queue send failed: {}", ret);

	debug_log("Testing queue, blocked on send");
	async([=]() mutable {
		sleep(1);
		int     val;
		Timeout noWait{0};
		int     ret = queue_receive(&noWait, &queue, &val);
		TEST(ret == 0, "Background receive failed: {}", ret);
		TEST(val == 0, "Background receive returned incorrect value: {}", ret);
		debug_log("Background thread made queue ready to send");
	});
	multiwaiter_queue_send_init(&events[0], &queue);
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "multiwaiter returned {}, expected 0", ret);
	TEST(events[0].value == 1, "Queue reports not ready");

	debug_log("Testing queue, blocked on receive");
	async([=]() mutable {
		sleep(1);
		int     val = 1;
		Timeout noWait{0};
		int     ret = queue_send(&noWait, &queue, &val);
		TEST(ret == 0, "Background send failed: {}", ret);
		debug_log("Background thread made queue ready to receive");
	});
	multiwaiter_queue_receive_init(&events[0], &queue);
	t   = 10;
	ret = multiwaiter_wait(&t, mw, events, 1);
	TEST(ret == 0, "multiwaiter returned {}, expected 0", ret);
	TEST(events[0].value == 1, "Queue did not return ready to receive");
	ret = queue_receive(&noWait, &queue, &val);
	TEST(ret == 0, "Queue ready to receive but receive returned {}", ret);
	TEST(val == 1, "Incorrect value returned from queue: {}", val);

	debug_log("Testing waiting on a queue and a futex");
	futex = 0;
	setFutex(&futex, 1);
	multiwaiter_queue_receive_init(&events[0], &queue);
	events[1]   = {&futex, EventWaiterFutex, 0};
	t.remaining = 6;
	ret         = multiwaiter_wait(&t, mw, events, 2);
	TEST(ret == 0, "multiwait on futex and queue returned {}", ret);
	TEST(events[0].value == 0,
	     "Queue reports ready to receive but should be empty.");
	TEST(events[1].value == 1, "Futex reports no wake");

	free(queueMemory);
	multiwaiter_delete(MALLOC_CAPABILITY, mw);
}

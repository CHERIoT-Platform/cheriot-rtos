// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Queue"
#include "tests.hh"
#include <debug.hh>
#include <errno.h>
#include <futex.h>
#include <queue.h>
#include <thread_pool.h>
#include <timeout.h>

using thread_pool::async;
static constexpr size_t ItemSize                    = 8;
static constexpr size_t MaxItems                    = 2;
static constexpr char   Message[MaxItems][ItemSize] = {"TstMsg0", "TstMsg1"};

void test_queue()
{
	static uint32_t futex;
	char            bytes[ItemSize];
	static void    *queue;
	Timeout         timeout{0, 0};
	debug_log("Testing queue send operations");
	int rv =
	  queue_create(&timeout, MALLOC_CAPABILITY, &queue, ItemSize, MaxItems);
	TEST(rv == 0, "Queue creation failed with {}", rv);
	rv = queue_send(&timeout, queue, Message[0]);
	TEST(rv == 0, "Sending the first message failed with {}", rv);
	rv = queue_send(&timeout, queue, Message[1]);
	TEST(rv == 0, "Sending the second message failed with {}", rv);
	// Queue is full, it should time out.
	timeout.remaining = 5;
	rv                = queue_send(&timeout, queue, Message[1]);
	TEST(rv == -ETIMEDOUT,
	     "Sending to a full queue didn't time out as expected, returned {}",
	     rv);
	debug_log("Testing queue receive operations");
	timeout.remaining = 10;
	rv                = queue_recv(&timeout, queue, bytes);
	TEST(rv == 0, "Receiving the first message failed with {}", rv);
	TEST(memcmp(Message[0], bytes, ItemSize) == 0,
	     "First message received but not as expected. Got {}",
	     bytes);
	rv = queue_recv(&timeout, queue, bytes);
	TEST(rv == 0, "Receiving the second message failed with {}", rv);
	TEST(memcmp(Message[1], bytes, ItemSize) == 0,
	     "Second message received but not as expected. Got {}",
	     bytes);
	timeout.remaining = 5;
	rv                = queue_recv(&timeout, queue, bytes);
	TEST(
	  rv == -ETIMEDOUT,
	  "Receiving from an empty queue didn't time out as expected, returned {}",
	  rv);
	debug_log("Testing queue deletion right underneath another thread");
	async([]() {
		char    bytesForAsync[ItemSize];
		Timeout infinity{0, UnlimitedTimeout};
		/*
		 * This test waits on a queue indefinitely, but will be woken up by the
		 * queue_delete() in the main thread. After waking up, the queue object
		 * is dead and will trap, causing a force unwind in the scheduler
		 * compartment.
		 */
		int rv = queue_recv(&infinity, queue, bytesForAsync);
		TEST(
		  rv == -1,
		  "queue_recv() should return -1 because the queue was freed() "
		  "underneath us and will trap then force unwind, but instead got {}",
		  rv);
		futex = 1;
		futex_wake(&futex, 1);
	});
	timeout.remaining = 20;
	thread_sleep(&timeout);
	rv = queue_delete(MALLOC_CAPABILITY, queue);
	TEST(rv == 0, "Queue deletion failed with {}", rv);
	// Wait until the async is done.
	rv = futex_wait(&futex, 0);
	TEST(rv == 0, "futex_wait() failed with {}", rv);
	debug_log("All queue tests successful");
}

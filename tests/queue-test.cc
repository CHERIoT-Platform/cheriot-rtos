// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment.h"
#include "token.h"
#include <cstdlib>
#define TEST_NAME "Queue"
#include "tests.hh"
#include <debug.hh>
#include <errno.h>
#include <queue.h>
#include <timeout.h>

static constexpr size_t ItemSize                    = 8;
static constexpr size_t MaxItems                    = 2;
static constexpr char   Message[MaxItems][ItemSize] = {"TstMsg0", "TstMsg1"};

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("Thread {} error handler invoked with mcause {}.  PCC: {}",
	          thread_id_get(),
	          mcause,
	          frame->pcc);
	return ErrorRecoveryBehaviour::ForceUnwind;
}

void test_queue_unsealed()
{
	char               bytes[ItemSize];
	static QueueHandle queue;
	static void       *queueMemory;
	Timeout            timeout{0, 0};
	debug_log("Testing queue send operations");
	auto checkSpace = [&](size_t         expected,
	                      SourceLocation loc = SourceLocation::current()) {
		size_t items;
		queue_items_remaining(&queue, &items);
		TEST(items == expected,
		     "Queue test line {} reports {} items, should contain {}",
		     loc.line(),
		     items,
		     expected);
	};
	int rv = queue_create(
	  &timeout, MALLOC_CAPABILITY, &queue, &queueMemory, ItemSize, MaxItems);
	TEST(queue.elementSize == ItemSize,
	     "Queue element size is {}, expected {}",
	     queue.elementSize,
	     ItemSize);
	TEST(queue.queueSize == MaxItems,
	     "Queue size is {}, expected {}",
	     queue.queueSize,
	     MaxItems);
	TEST(rv == 0, "Queue creation failed with {}", rv);
	rv = queue_send(&timeout, &queue, Message[0]);
	checkSpace(1);
	TEST(rv == 0, "Sending the first message failed with {}", rv);
	checkSpace(1);
	rv = queue_send(&timeout, &queue, Message[1]);
	TEST(rv == 0, "Sending the second message failed with {}", rv);
	checkSpace(2);
	// Queue is full, it should time out.
	timeout.remaining = 5;
	rv                = queue_send(&timeout, &queue, Message[1]);
	TEST(rv == -ETIMEDOUT,
	     "Sending to a full queue didn't time out as expected, returned {}",
	     rv);
	checkSpace(2);
	debug_log("Testing queue receive operations");
	timeout.remaining = 10;
	rv                = queue_receive(&timeout, &queue, bytes);
	TEST(rv == 0, "Receiving the first message failed with {}", rv);
	TEST(memcmp(Message[0], bytes, ItemSize) == 0,
	     "First message received but not as expected. Got {}",
	     bytes);
	checkSpace(1);
	rv = queue_receive(&timeout, &queue, bytes);
	TEST(rv == 0, "Receiving the second message failed with {}", rv);
	TEST(memcmp(Message[1], bytes, ItemSize) == 0,
	     "Second message received but not as expected. Got {}",
	     bytes);
	checkSpace(0);
	timeout.remaining = 5;
	rv                = queue_receive(&timeout, &queue, bytes);
	TEST(
	  rv == -ETIMEDOUT,
	  "Receiving from an empty queue didn't time out as expected, returned {}",
	  rv);
	// Check that the items remaining calculations are correct after overflow.
	queue_send(&timeout, &queue, Message[1]);
	checkSpace(1);
	queue_receive(&timeout, &queue, bytes);
	checkSpace(0);
	queue_send(&timeout, &queue, Message[1]);
	checkSpace(1);
	queue_receive(&timeout, &queue, bytes);
	checkSpace(0);
	rv = queue_destroy(MALLOC_CAPABILITY, &queue);
	TEST(rv == 0, "Queue deletion failed with {}", rv);
	debug_log("All queue library tests successful");
}

void test_queue_sealed()
{
	size_t  heapSpace = heap_quota_remaining(MALLOC_CAPABILITY);
	Timeout t{1};
	SObj    receiveHandle;
	SObj    sendHandle;
	char    bytes[ItemSize];
	int     ret = queue_create_sealed(
	      &t, MALLOC_CAPABILITY, &sendHandle, &receiveHandle, ItemSize, MaxItems);
	TEST(ret == 0, "Queue creation failed with {}", ret);

	t   = UnlimitedTimeout;
	ret = queue_send_sealed(&t, receiveHandle, Message[1]);
	TEST(
	  ret == -EINVAL,
	  "Sending with a receive handle should return -EINVAL ({}), returned {}",
	  EINVAL,
	  ret);
	ret = queue_receive_sealed(&t, sendHandle, bytes);
	TEST(
	  ret == -EINVAL,
	  "Sending with a receive handle should return -EINVAL ({}), returned {}",
	  EINVAL,
	  ret);

	ret = queue_send_sealed(&t, sendHandle, Message[1] + 1);
	TEST(ret == -EPERM,
	     "Sending with short buffer should return -EPERM ({}), returned {}",
	     EPERM,
	     ret);
	ret = queue_send_sealed(&t, sendHandle, Message[1]);
	TEST(
	  ret == 0, "Sending with valid buffer should return 0, returned {}", ret);
	ret = queue_receive_sealed(&t, receiveHandle, bytes + 1);
	TEST(ret == -EPERM,
	     "Receiving with short buffer should return -EPERM ({}), returned {}",
	     EPERM,
	     ret);
	size_t items;
	ret = queue_items_remaining_sealed(receiveHandle, &items);
	TEST(ret == 0, "Getting items remaining should return 0, returned {}", ret);
	TEST(items == 1, "Items remaining should be 1, is {}", items);
	ret = queue_items_remaining_sealed(sendHandle, &items);
	TEST(ret == 0, "Getting items remaining should return 0, returned {}", ret);
	TEST(items == 1, "Items remaining should be 1, is {}", items);
	ret = queue_receive_sealed(&t, receiveHandle, bytes);
	TEST(ret == 0,
	     "Receiving with valid buffer should return 0, returned {}",
	     ret);

	// Put something in the queue before we delete the send handle.
	ret = queue_send_sealed(&t, sendHandle, Message[1]);
	TEST(
	  ret == 0, "Sending with valid buffer should return 0, returned {}", ret);

	t   = 1;
	ret = queue_destroy_sealed(&t, MALLOC_CAPABILITY, sendHandle);
	TEST(ret == 0, "Queue send destruction failed with {}", ret);

	t   = 1;
	ret = queue_destroy_sealed(&t, MALLOC_CAPABILITY, receiveHandle);
	TEST(ret == 0, "Queue receive destruction failed with {}", ret);

	TEST(heap_quota_remaining(MALLOC_CAPABILITY) == heapSpace,
	     "Heap space leaked");
	debug_log("All queue compartment tests successful");
}

void test_queue()
{
	test_queue_unsealed();
	test_queue_sealed();
	debug_log("All queue tests successful");
}

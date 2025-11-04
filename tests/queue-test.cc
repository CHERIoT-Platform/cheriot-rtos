// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment.h"
#include "token.h"
#include <cstdlib>
#include <errno.h>
#define TEST_NAME "MessageQueue"
#include "tests.hh"
#include <FreeRTOS-Compat/queue.h>
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

void test_queue_multiple()
{
	static MessageQueue *queue;
	Timeout              timeout{0};
	const size_t         QueueSize  = 9;
	const size_t         BufferSize = 4;
	int rv = queue_create(&timeout, MALLOC_CAPABILITY, &queue, 1, QueueSize);
	TEST(rv == 0, "MessageQueue creation failed with {}", rv);
	char next       = 0;
	auto fillBuffer = [&](char *buffer) {
		for (int i = 0; i < BufferSize; i++)
		{
			buffer[i] = next++;
		}
	};
	auto checkReceived = [](char *expected, char *received) {
		for (int i = 0; i < BufferSize; i++)
		{
			TEST(expected[i] == received[i],
			     "{} != {} in byte {}",
			     static_cast<int>(expected[i]),
			     static_cast<int>(received[i]),
			     i);
		}
	};
	for (int i = 0; i < 10; i++)
	{
		char buffer1[BufferSize];
		char buffer2[BufferSize];
		char receiveBuffer[BufferSize];
		fillBuffer(buffer1);
		fillBuffer(buffer2);

		int sent = queue_send_multiple(&timeout, queue, buffer1, BufferSize);
		TEST_EQUAL(sent, BufferSize, " failed to send (first time)");
		sent = queue_send_multiple(&timeout, queue, buffer2, BufferSize);
		TEST_EQUAL(sent, BufferSize, " failed to send (second time)");

		int received =
		  queue_receive_multiple(&timeout, queue, receiveBuffer, BufferSize);
		TEST_EQUAL(received, BufferSize, " failed to receive");
		checkReceived(buffer1, receiveBuffer);

		received =
		  queue_receive_multiple(&timeout, queue, receiveBuffer, BufferSize);
		TEST_EQUAL(received, BufferSize, " failed to receive (second time)");
		checkReceived(buffer2, receiveBuffer);
	}

	TEST_EQUAL(queue_destroy(MALLOC_CAPABILITY, queue),
	           0,
	           "MessageQueue deletion failed");
}

void test_queue_unsealed()
{
	char                 bytes[ItemSize];
	static MessageQueue *queue;
	Timeout              timeout{0, 0};
	debug_log("Testing queue send operations");
	auto checkSpace = [&](size_t         expected,
	                      SourceLocation loc = SourceLocation::current()) {
		size_t items;
		queue_items_remaining(queue, &items);
		TEST(items == expected,
		     "MessageQueue test line {} reports {} items, should contain {}",
		     loc.line(),
		     items,
		     expected);
	};
	int rv =
	  queue_create(&timeout, MALLOC_CAPABILITY, &queue, ItemSize, MaxItems);
	TEST(queue->elementSize == ItemSize,
	     "MessageQueue element size is {}, expected {}",
	     queue->elementSize,
	     ItemSize);
	TEST(queue->queueSize == MaxItems,
	     "MessageQueue size is {}, expected {}",
	     queue->queueSize,
	     MaxItems);
	TEST(rv == 0, "MessageQueue creation failed with {}", rv);
	rv = queue_send(&timeout, queue, Message[0]);
	checkSpace(1);
	TEST(rv == 0, "Sending the first message failed with {}", rv);
	checkSpace(1);
	rv = queue_send(&timeout, queue, Message[1]);
	TEST(rv == 0, "Sending the second message failed with {}", rv);
	checkSpace(2);
	// MessageQueue is full, it should time out.
	timeout.remaining = 5;
	rv                = queue_send(&timeout, queue, Message[1]);
	TEST(rv == -ETIMEDOUT,
	     "Sending to a full queue didn't time out as expected, returned {}",
	     rv);
	checkSpace(2);
	debug_log("Testing queue receive operations");
	timeout.remaining = 10;
	rv                = queue_receive(&timeout, queue, bytes);
	TEST(rv == 0, "Receiving the first message failed with {}", rv);
	TEST(memcmp(Message[0], bytes, ItemSize) == 0,
	     "First message received but not as expected. Got {}",
	     bytes);
	checkSpace(1);
	rv = queue_receive(&timeout, queue, bytes);
	TEST(rv == 0, "Receiving the second message failed with {}", rv);
	TEST(memcmp(Message[1], bytes, ItemSize) == 0,
	     "Second message received but not as expected. Got {}",
	     bytes);
	checkSpace(0);
	timeout.remaining = 5;
	rv                = queue_receive(&timeout, queue, bytes);
	TEST(
	  rv == -ETIMEDOUT,
	  "Receiving from an empty queue didn't time out as expected, returned {}",
	  rv);
	// Check that the items remaining calculations are correct after overflow.
	queue_send(&timeout, queue, Message[1]);
	checkSpace(1);
	queue_receive(&timeout, queue, bytes);
	checkSpace(0);
	queue_send(&timeout, queue, Message[1]);
	checkSpace(1);
	queue_receive(&timeout, queue, bytes);
	checkSpace(0);
	rv = queue_destroy(MALLOC_CAPABILITY, queue);
	TEST(rv == 0, "MessageQueue deletion failed with {}", rv);
	/* The queue implementation has a size limit due to using the top bits of
	 * counters as size */
	TEST_EQUAL(
	  queue_create(&timeout, MALLOC_CAPABILITY, &queue, 0x1, 0x20000000),
	  -EINVAL,
	  "queue_create with too large size not return -EINVAL");
	/* queue_create should detect 32-bit overflow and fail */
	TEST_EQUAL(
	  queue_create(&timeout, MALLOC_CAPABILITY, &queue, 0x10000, 0x10000),
	  -EINVAL,
	  "queue_create with 32-bit overflow did not return -EINVAL");
	/* queue_create should detect signed overflow and fail */
	TEST_EQUAL(
	  queue_create(&timeout, MALLOC_CAPABILITY, &queue, 0x10000, 0x8000),
	  -EINVAL,
	  "queue_create with signed overflow did not return -EINVAL");
	debug_log("All queue library tests successful");
}

void test_queue_sealed()
{
	auto    heapSpace = heap_quota_remaining(MALLOC_CAPABILITY);
	Timeout t{1};
	CHERI_SEALED(struct MessageQueue *) receiveHandle;
	CHERI_SEALED(struct MessageQueue *) sendHandle;
	CHERI_SEALED(struct MessageQueue *) queue;
	char bytes[ItemSize];
	int  ret =
	  queue_create_sealed(&t, MALLOC_CAPABILITY, &queue, ItemSize, MaxItems);
	TEST(ret == 0, "MessageQueue creation failed with {}", ret);

	// The next APIs are deprecated.  Make sure that we haven't broken them.
	__clang_ignored_warning_push("-Wdeprecated");
	ret = queue_receive_handle_create_sealed(
	  &t, MALLOC_CAPABILITY, queue, &receiveHandle);
	TEST(
	  ret == 0, "MessageQueue receive endpoint creation failed with {}", ret);
	ret = queue_send_handle_create_sealed(
	  &t, MALLOC_CAPABILITY, queue, &sendHandle);
	TEST(ret == 0, "MessageQueue send endpoint creation failed with {}", ret);
	__clang_ignored_warning_pop();

	t   = UnlimitedTimeout;
	ret = queue_send_sealed(&t, receiveHandle, Message[1]);
	TEST_EQUAL(ret,
	           -EPERM,
	           "Sending with a receive handle should have failed with -EPERM");

	ret = queue_receive_sealed(&t, sendHandle, bytes);
	TEST_EQUAL(
	  ret,
	  -EPERM,
	  "Sending with a receive handle should return -EPERM ({}), returned {}");

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

	sendHandle = queue_permissions_and(sendHandle, MessageQueuePermitSend);

	t   = 1;
	ret = queue_destroy_sealed(&t, MALLOC_CAPABILITY, sendHandle);
	TEST_EQUAL(ret,
	           -EPERM,
	           "MessageQueue send destruction should have failed with -EPERM");

	t   = 1;
	ret = queue_destroy_sealed(&t, nullptr, queue);
	TEST_EQUAL(ret,
	           -EPERM,
	           "MessageQueue send destruction should have failed with -EPERM");

	t   = 1;
	ret = queue_destroy_sealed(&t, MALLOC_CAPABILITY, queue);
	TEST(ret == 0, "MessageQueue destruction failed with {}", ret);

	TEST(heap_quota_remaining(MALLOC_CAPABILITY) == heapSpace,
	     "Heap space leaked");
	debug_log("All queue compartment tests successful");
}

void test_queue_freertos()
{
	debug_log("Testing FreeRTOS queues");
	auto quotaBegin    = heap_quota_remaining(MALLOC_CAPABILITY);
	auto freertosQueue = xQueueCreate(10, sizeof(int));
	vQueueDelete(freertosQueue);
	auto quotaEnd = heap_quota_remaining(MALLOC_CAPABILITY);
	TEST(
	  quotaBegin == quotaEnd,
	  "The FreeRTOS queue wrapper leaks memory: quota before is {}, after {}",
	  quotaBegin,
	  quotaEnd);
	debug_log("All FreeRTOS queue tests successful");
}

int test_queue()
{
	test_queue_unsealed();
	test_queue_multiple();
	test_queue_sealed();
	test_queue_freertos();
	debug_log("All queue tests successful");
	return 0;
}

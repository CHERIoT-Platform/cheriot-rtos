// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "consumer.h"
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <queue.h>
#include <timeout.hh>
#include <token.h>

using Debug = ConditionalDebug<true, "Producer">;

/**
 * Run the producer thread, sending integers to the consumer.
 */
int __cheri_compartment("producer") run()
{
	// Allocate the queue
	CHERI_SEALED(struct MessageQueue *) queue;
	int result = non_blocking<queue_create_sealed>(
	  MALLOC_CAPABILITY, &queue, sizeof(int), 16);
	// Check for allocation failure.
	Debug::Invariant(result == 0,
	                 "Compartment call to queue_create_sealed failed: {}",
	                 result);
	// Pass the queue handle to the consumer.
	Debug::Invariant(set_queue(queue) == 0,
	                 "Compartment call to set_queue failed");
	Debug::log("Starting producer loop");
	// Loop, sending some numbers to the other thread.
	for (int i = 1; i < 200; i++)
	{
		int ret = blocking_forever<queue_send_sealed>(queue, &i);
		// Abort if the queue send errors.
		Debug::Invariant(ret == 0, "Queue send failed {}", ret);
	}
	Debug::log("Producer sent all messages to consumer");
	return 0;
}

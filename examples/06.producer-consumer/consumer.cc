// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "consumer.h"
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <futex.h>
#include <queue.h>

using Debug = ConditionalDebug<true, "Consumer">;

// The queue that we will wait on.
void *queue;

/**
 * Set the queue that the thread in this compartment will use.
 */
void set_queue(void *newQueue)
{
	// Check that this is a valid queue
	size_t items;
	if (queue_items_remaining(newQueue, &items) != 0)
	{
		return;
	}
	// Set it in the global and allow the thread to start.
	queue = newQueue;
	Debug::log("Queue set to {}", queue);
	futex_wake(reinterpret_cast<uint32_t *>(&queue), 1);
}

/**
 * Run loop for the consumer thread.
 */
void __cheri_compartment("consumer") run()
{
	// Use the queue pointer as a futex.  It is initialised to 0, if the other
	// thread has stored a valid pointer here then it will not be zero and so
	// futex_wait will return immediately.
	futex_wait(reinterpret_cast<uint32_t *>(&queue), 0);
	Debug::log("Waiting for messages");
	// Get a message from the queue and print it.  This blocks indefinitely.
	int     value = 0;
	Timeout t{UnlimitedTimeout};
	while ((value != 199) && (queue_recv(queue, &value, &t) == 0))
	{
		Debug::log("Read {} from queue", value);
	}
}

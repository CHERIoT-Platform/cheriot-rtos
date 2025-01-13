// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "consumer.h"
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <futex.h>
#include <queue.h>
#include <timeout.hh>
#include <token.h>

using Debug = ConditionalDebug<true, "Consumer">;

// The queue that we will wait on.
CHERI_SEALED(struct MessageQueue *) queue;

/**
 * Set the queue that the thread in this compartment will use.
 */
int set_queue(CHERI_SEALED(struct MessageQueue *) newQueue)
{
	// Check that this is a valid queue
	size_t items;
	if (queue_items_remaining_sealed(newQueue, &items) != 0)
	{
		return -1;
	}
	// Set it in the global and allow the thread to start.
	queue = newQueue;
	Debug::log("Queue set to {}", queue);
	Debug::Invariant(futex_wake(reinterpret_cast<uint32_t *>(&queue), 1) !=
	                   -ECOMPARTMENTFAIL,
	                 "Compartment call to futex_wake failed");
	return 0;
}

/**
 * Run loop for the consumer thread.
 */
int __cheri_compartment("consumer") run()
{
	// Use the queue pointer as a futex.  It is initialised to 0, if the other
	// thread has stored a valid pointer here then it will not be zero and so
	// futex_wait will return immediately.
	Debug::Invariant(futex_wait(reinterpret_cast<uint32_t *>(&queue), 0) !=
	                   -ECOMPARTMENTFAIL,
	                 "Compartment call to futex_wake failed");
	Debug::log("Waiting for messages");
	// Get a message from the queue and print it.  This blocks indefinitely.
	int     value = 0;
	Timeout t{UnlimitedTimeout};
	while ((value != 199) && (queue_receive_sealed(&t, queue, &value) == 0))
	{
		Debug::log("Read {} from queue", value);
	}
	return 0;
}

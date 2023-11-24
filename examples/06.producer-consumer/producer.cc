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
void __cheri_compartment("producer") run()
{
	// Allocate the queue
	SObj sendHandle;
	SObj receiveHandle;
	non_blocking<queue_create_sealed>(
	  MALLOC_CAPABILITY, &sendHandle, &receiveHandle, sizeof(int), 16);
	// Pass the queue handle to the consumer.
	set_queue(receiveHandle);
	Debug::log("Starting producer loop");
	// Loop, sending some numbers to the other thread.
	for (int i = 1; i < 200; i++)
	{
		int ret = blocking_forever<queue_send_sealed>(sendHandle, &i);
		// Abort if the queue send errors.
		Debug::Invariant(ret == 0, "Queue send failed {}", ret);
	}
	Debug::log("Producer sent all messages to consumer");
}

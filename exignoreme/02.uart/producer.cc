// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// #include "compartment-macros.h"
#include <cstdint>
#include <timeout.hh>
#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <queue.h>
#include "uart2.hh"

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Producer compartment">;

/// Thread entry point.
void __cheri_compartment("producer") main_entry()
{
	// Print welcome, along with the compartment's name to the default UART.
	Debug::log("Generate Messages for Queue");
	// Allocate the queue
	CHERI_SEALED(MessageQueue *) queue;
	non_blocking<queue_create_sealed>(MALLOC_CAPABILITY, &queue, sizeof(int), 16);
	// Pass the queue handle to the consumer.
	set_queue(queue);

	Debug::log("Start producer loop");
	uint32_t message = 0;
	while(true) {
		thread_millisecond_wait(60000);	// sleep for 1 minute
		int ret = blocking_forever<queue_send_sealed>(queue, &message);
		// Abort if the queue send errors.
		if(ret != 0) {
			Debug::log("Queue send failed {}", ret);
		} else {
			message++;	// Only increment if it worked.
		}
	}
}

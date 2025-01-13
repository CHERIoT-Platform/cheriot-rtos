// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <debug.hh>
#include <fail-simulator-on-error.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "UART compartment">;

/// Write a message to the UART.
[[cheri::interrupt_state(disabled)]] int write(const char *msg)
{
	// Print the message.
	Debug::log("{}", msg);
	return 0;
}

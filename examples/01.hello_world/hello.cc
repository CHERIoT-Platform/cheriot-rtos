// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Hello world compartment">;

/// Thread entry point.
void __cheri_compartment("hello") say_hello()
{
	// Print hello world, along with the compartment's name to the default UART.
	Debug::log("Hello world");
}

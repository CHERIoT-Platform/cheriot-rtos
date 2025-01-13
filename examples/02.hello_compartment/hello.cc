// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <debug.hh>

// This header adds an error handler that writes to the UART on error.
// Uncomment it and see that the compartmentalisation policy no longer passes.
// #include <fail-simulator-on-error.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Hello compartment">;

/// Thread entry point.
int __cheri_compartment("hello") entry()
{
	Debug::Invariant(write("Hello world") != -ECOMPARTMENTFAIL,
	                 "Compartment call to write failed");
	char stackBuffer[] = "Hello from the stack";
	Debug::Invariant(write(stackBuffer) != -ECOMPARTMENTFAIL,
	                 "Compartment call to write failed");
	return 0;
}

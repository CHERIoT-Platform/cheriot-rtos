// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
// This header adds an error handler that writes to the UART on error.
// Uncomment it and see that the compartmentalisation policy no longer passes.
// #include <fail-simulator-on-error.h>

/// Thread entry point.
void __cheri_compartment("hello") entry()
{
	write("Hello world");
	char stackBuffer[] = "Hello from the stack";
	write(stackBuffer);
}

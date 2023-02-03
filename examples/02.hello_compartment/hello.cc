// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <fail-simulator-on-error.h>

/// Thread entry point.
void __cheri_compartment("hello") entry()
{
	write("Hello world");
	char stackBuffer[] = "Hello from the stack";
	write(stackBuffer);
}

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Allocating compartment">;

/// Thread entry point.
void __cheri_compartment("allocate") entry()
{
	void *x = malloc(42);
	// Print the allocated value:
	Debug::log("Allocated: {}", x);
	free(x);
	// Print the dangling pointer, note that it is no longer a valid pointer
	// (v:0)
	Debug::log("Use after free: {}", x);
}

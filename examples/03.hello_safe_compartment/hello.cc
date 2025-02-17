// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <debug.hh>
#include <fail-simulator-on-error.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Hello compartment">;

/// Thread entry point.
int __cheri_compartment("hello") entry()
{
	char maliciousString[] = {'h', 'e', 'l', 'l', 'o'};
	Debug::Invariant(write(maliciousString) == 0,
	                 "Compartment call to write failed");
	Debug::Invariant(write("Non-malicious string") == 0,
	                 "Compartment call to write failed");
	return 0;
}

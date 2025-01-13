// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <debug.hh>
#include <fail-simulator-on-error.h>

using Debug = ConditionalDebug<true, "hello compartment">;

/// Thread entry point.
int __cheri_compartment("hello") entry()
{
	// Try writing a string with a missing null terminator
	char maliciousString[] = {'h', 'e', 'l', 'l', 'o'};
	(void)write(maliciousString);
	// Now try one that doesn't have read permission:
	CHERI::Capability storeOnlyString{maliciousString};
	storeOnlyString.permissions() &= CHERI::Permission::Store;
	(void)write(storeOnlyString);
	// Now one that should work
	Debug::Invariant(write("Non-malicious string") != -ECOMPARTMENTFAIL,
	                 "Compartment call to write failed");
	return 0;
}

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <fail-simulator-on-error.h>

/// Thread entry point.
void __cheri_compartment("hello") entry()
{
	// Try writing a string with a missing null terminator
	char maliciousString[] = {'h', 'e', 'l', 'l', 'o'};
	write(maliciousString);
	// Now try one that doesn't have read permission:
	CHERI::Capability storeOnlyString{maliciousString};
	storeOnlyString.permissions() &= CHERI::Permission::Store;
	write(storeOnlyString);
	// Now one that should work
	write("Non-malicious string");
}

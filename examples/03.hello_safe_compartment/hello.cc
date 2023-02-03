// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <fail-simulator-on-error.h>

/// Thread entry point.
void __cheri_compartment("hello") entry()
{
	char maliciousString[] = {'h', 'e', 'l', 'l', 'o'};
	write(maliciousString);
	write("Non-malicious string");
}

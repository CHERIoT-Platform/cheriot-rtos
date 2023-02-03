// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "identifier.h"
#include <debug.hh>
#include <fail-simulator-on-error.h>

using Debug = ConditionalDebug<true, "Caller compartment">;

/// Thread entry point.
void __cheri_compartment("caller") entry()
{
	auto *identifier = identifier_create(42);
	Debug::log("Allocated identifier to hold the value 42: {}", identifier);
	Debug::log("Value is {}", identifier_value(identifier));
	identifier_destroy(identifier);
	Debug::log("Dangling pointer: {}", identifier);
}

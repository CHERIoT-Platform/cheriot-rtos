// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "caesar_cypher.h"
#include <debug.hh>

using Debug = ConditionalDebug<true, "Entry compartment">;

char buffer[1024];

/// Thread entry point.
void __cheri_compartment("entry") entry()
{
	ssize_t length = produce_message(buffer, sizeof(buffer));
	if (length < 0)
	{
		Debug::log("Failed to get encrypted message");
		return;
	}
	Debug::log("Received encrypted message: '{}' ({} bytes)",
	           std::string_view{buffer, static_cast<size_t>(length)},
	           length);
	consume_message(buffer, length);
}

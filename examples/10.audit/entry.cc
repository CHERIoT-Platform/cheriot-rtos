// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "caesar_cypher.h"
#include <debug.hh>
#include <errno.h>

using Debug = ConditionalDebug<true, "Entry compartment">;

char buffer[1024];

/// Thread entry point.
int __cheri_compartment("entry") entry()
{
	ssize_t length = produce_message(buffer, sizeof(buffer));
	if (length < 0)
	{
		Debug::log("Failed to get encrypted message");
		return -1;
	}
	Debug::log("Received encrypted message: '{}' ({} bytes)",
	           std::string_view{buffer, static_cast<size_t>(length)},
	           length);
	Debug::Invariant(consume_message(buffer, length) == 0,
	                 "Compartment call to consume_message failed");
	return 0;
}

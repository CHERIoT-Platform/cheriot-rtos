// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "caesar_cypher.h"
#include <debug.hh>
#include <errno.h>

using Debug = ConditionalDebug<true, "Consumer">;

DECLARE_AND_DEFINE_CAESAR_CAPABILITY(encrypt, false, true, 95);

int consume_message(const char *buffer, size_t length)
{
	std::string decrypted;
	decrypted.resize(length);
	Debug::Invariant(
	  caesar_decrypt(
	    STATIC_SEALED_VALUE(encrypt), buffer, decrypted.data(), length) == 0,
	  "Compartment call to caesar_decrypt failed");
	Debug::log("Decrypted message: '{}'", decrypted);
	return 0;
}

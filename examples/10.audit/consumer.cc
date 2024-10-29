// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "caesar_cypher.h"
#include <debug.hh>
#include <errno.h>

using Debug = ConditionalDebug<true, "Consumer">;

DECLARE_AND_DEFINE_CAESAR_CAPABILITY(encrypt, false, true, 95);

void consume_message(const char *buffer, size_t length)
{
	std::string decrypted;
	decrypted.resize(length);
	caesar_decrypt(
	  STATIC_SEALED_VALUE(encrypt), buffer, decrypted.data(), length);
	Debug::log("Decrypted message: '{}'", decrypted);
}

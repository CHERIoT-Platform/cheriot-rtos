// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "caesar_cypher.h"
#include <debug.hh>
#include <errno.h>

using Debug = ConditionalDebug<true, "Producer">;

DECLARE_AND_DEFINE_CAESAR_CAPABILITY(encrypt, true, false, 95);

ssize_t produce_message(char *buffer, size_t length)
{
	const std::string_view Plaintext = "Hello, World!";
	if (length < Plaintext.size())
	{
		return -ENOMEM;
	}
	Debug::log("Encrypting message '{}'", Plaintext);
	int ret = caesar_encrypt(
	  STATIC_SEALED_VALUE(encrypt), Plaintext.data(), buffer, Plaintext.size());
	if (ret < 0)
	{
		return ret;
	}
	return Plaintext.size();
}

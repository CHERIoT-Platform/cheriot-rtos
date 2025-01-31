#include "caesar_cypher.h"
#include <compartment-macros.h>
#include <debug.hh>
#include <errno.h>

using Debug = ConditionalDebug<true, "Caesar">;

int caesar_encrypt(CHERI_SEALED(struct CaesarCapability *) capability,
                   const char *input,
                   char       *output,
                   size_t      length)
{
	auto *cypherState = token_unseal<CaesarCapability>(
	  STATIC_SEALING_TYPE(CaesarCapabilityType), capability);
	if (cypherState == nullptr)
	{
		return -EINVAL;
	}
	if (!cypherState->permitEncrypt)
	{
		return -EPERM;
	}
	for (int i = 0; i < length; i++)
	{
		uint8_t c = input[i];
		// Replace control characters with 'X'
		if ((c < 32) || (c >= 127))
		{
			c = 'X';
		}
		// Reset the base to 0.
		c -= 32;
		// Perform the encryption
		c += cypherState->shift;
		if (c > 94)
		{
			c -= 94;
		}
		c += 32;
		output[i] = c;
	}
	return 0;
}

int caesar_decrypt(CHERI_SEALED(struct CaesarCapability *) capability,
                   const char *input,
                   char       *output,
                   size_t      length)
{
	auto *cypherState = token_unseal<CaesarCapability>(
	  STATIC_SEALING_TYPE(CaesarCapabilityType), capability);
	if (cypherState == nullptr)
	{
		return -EINVAL;
	}
	if (!cypherState->permitDecrypt)
	{
		return -EPERM;
	}
	for (int i = 0; i < length; i++)
	{
		int c = input[i];
		// Perform the encryption
		c -= cypherState->shift;
		if (c < 32)
		{
			c += 94;
		}
		output[i] = c;
	}
	return 0;
}

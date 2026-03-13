// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <string.h>

void *__cheri_libcall memccpy(void       *destVoid,
                              const void *srcVoid,
                              int         cInt,
                              size_t      n)
{
	uint8_t       *d = (uint8_t *)destVoid;
	const uint8_t *s = (const uint8_t *)srcVoid;
	uint8_t        c = (uint8_t)cInt;

	while (n--)
	{
		*d = *s;
		// Stop when character c is found
		if (*s == c)
		{
            return d + 1;
		}
		d++;
		s++;
	}

    return NULL;
}

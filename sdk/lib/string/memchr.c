// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <string.h>

void *__cheri_libcall memchr(const void *voidString,
                             int         intChar,
                             size_t      n)
{
	const unsigned char  c = (unsigned char)intChar;
	const unsigned char *s = (const unsigned char *)voidString;

	for (size_t i = 0; i != n; i++)
	{
		if (*s == c)
		{
			return (void *)s;
		}
		s++;
	}

	return NULL;
}

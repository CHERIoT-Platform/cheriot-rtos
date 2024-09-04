// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <string.h>

void *__cheri_libcall memrchr(const void *voidString,
                              int         intChar,
                              size_t      n)
{
	const unsigned char  c = (unsigned char)intChar;
	const unsigned char *s = (const unsigned char *)voidString;

  s += n;
	for (size_t i = n; i > 0; --i)
	{
		--s;
		if (*s == c)
		{
			return (void *)s;
		}
	}

	return NULL;
}

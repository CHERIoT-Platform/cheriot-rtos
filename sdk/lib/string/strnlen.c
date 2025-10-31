// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <stddef.h>
#include <string.h>

size_t strnlen(const char *str, size_t maxlen)
{
	size_t originalMaxlen = maxlen;
	while ((maxlen>0) && (*str != '\0'))
	{
		str++;
		maxlen--;
	}
	return originalMaxlen - maxlen;
}

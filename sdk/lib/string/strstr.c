// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <stddef.h>
#include <string.h>

char *__cheri_libcall strstr(const char *haystack, const char *needle)
{
	size_t needleLen = strlen(needle);

	while (*haystack != 0)
	{
		if (strncmp(haystack, needle, needleLen) == 0)
			return (char *)haystack;
		haystack++;
	}
	return NULL;
}

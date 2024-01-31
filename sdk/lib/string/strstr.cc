// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <stddef.h>
#include <string.h>

char *__cheri_libcall strnstr(const char *haystack,
                              const char *needle,
                              size_t      haystackLength)
{
	size_t needleLen = strlen(needle);

	while ((haystackLength > needleLen) && (*haystack != 0))
	{
		if (strncmp(haystack, needle, needleLen) == 0)
		{
			return const_cast<char *>(haystack);
		}
		haystack++;
		haystackLength--;
	}
	return nullptr;
}

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>

CHERIOT_DECLARE_STANDARD_LIBCALL(memcmp,
                                 int,
                                 const void *str1,
                                 const void *str2,
                                 size_t      count)
CHERIOT_DECLARE_STANDARD_LIBCALL(memcmp,
                                 int,
                                 const void *str1,
                                 const void *str2,
                                 size_t      count)
CHERIOT_DECLARE_STANDARD_LIBCALL(memcpy,
                                 void *,
                                 void       *dest,
                                 const void *src,
                                 size_t      n)
CHERIOT_DECLARE_STANDARD_LIBCALL(memset, void *, void *, int, size_t)
CHERIOT_DECLARE_STANDARD_LIBCALL(memmove,
                                 void *,
                                 void       *dest,
                                 const void *src,
                                 size_t      n)
CHERIOT_DECLARE_STANDARD_LIBCALL(memchr, void *, const void *, int, size_t)
CHERIOT_DECLARE_STANDARD_LIBCALL(memrchr, void *, const void *, int, size_t)
CHERIOT_DECLARE_STANDARD_LIBCALL(strlen, size_t, const char *str)
CHERIOT_DECLARE_STANDARD_LIBCALL(strncmp,
                                 int,
                                 const char *s1,
                                 const char *s2,
                                 size_t      n)
CHERIOT_DECLARE_STANDARD_LIBCALL(strncpy,
                                 char *,
                                 char       *dest,
                                 const char *src,
                                 size_t      n)
CHERIOT_DECLARE_STANDARD_LIBCALL(strcmp, int, const char *s1, const char *s2)
CHERIOT_DECLARE_STANDARD_LIBCALL(strnstr,
                                 char *,
                                 const char *haystack,
                                 const char *needle,
                                 size_t      haystackLength)
CHERIOT_DECLARE_STANDARD_LIBCALL(strchr, char *, const char *s, int c)
CHERIOT_DECLARE_STANDARD_LIBCALL(strlcpy,
                                 size_t,
                                 char       *dest,
                                 const char *src,
                                 size_t      n)

/**
 * Explicit bzero is a memset variant that the compiler is not permitted to
 * remove.  Our implementation simply wraps memset and is safe from removal
 * because it is provided by a different shared library.
 */
void __cheri_libcall explicit_bzero(void *s, size_t n);

__always_inline static inline char *strcpy(char *dst, const char *src)
{
	return dst + strlcpy(dst, src, SIZE_MAX);
}

__always_inline static inline char *strstr(const char *haystack,
                                           const char *needle)
{
	return strnstr(haystack, needle, SIZE_MAX);
}

#pragma once
/**
 * Minimal wchar.h  This exists because libc++ doesn't support platforms that
 * don't have it.  We don't expect anything to actually use the template
 * instantiations that would require anything in this file but the identifiers
 * must exist.
 */
#include <string.h>

/**
 * libc++ headers require wchar.h to exist with a definition of an mbstate_t
 * structure.  We don't use this with anything, so this is just a stub.
 */
struct mbstate_t
{
};

/**
 * `string_view` requires `wmemcpy` to exist.  Forward to `memcpy`.
 */
static inline wchar_t *wmemcpy(wchar_t *s1, const wchar_t *s2, size_t n)
{
	return (wchar_t *)memcpy(s1, s2, n / sizeof(wchar_t));
}

/**
 * `string_view` requires `wmemmove` to exist.  Forward to `memmove`.
 */
static inline wchar_t *wmemmove(wchar_t *s1, const wchar_t *s2, size_t n)
{
	return (wchar_t *)memmove(s1, s2, n / sizeof(wchar_t));
}

/**
 * `string_view` requires `wmemset` to exist.  Provide a trivial
 * implementation.
 */
static inline wchar_t *wmemset(wchar_t *dest, wchar_t ch, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		dest[i] = ch;
	}
	return dest;
}

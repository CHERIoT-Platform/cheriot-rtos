// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
#pragma once

/**
 * \file
 *
 * ASCII-only definition of C standard C functions.
 *
 * These functions are explicitly not locale aware.  Apart from `isascii`,
 * their return values are unspecified for non-ASCII character sets.  Most
 * embedded software does not need to be locale aware and talks to some back
 * end that handles localisation.  If you require support for Unicode or other
 * non-ASCII character sets, do not use this file.
 */

/**
 * Returns true if `c` is a 7-bit ASCII character, false otherwise.  The value
 * of `c` must be some ASCII-superset encoding, such as Unicode or an 8-bit code
 * page.
 */
static inline int isascii(int c)
{
	return (c & ~0x7F) == 0;
}

/**
 * Returns true if `c` is an ASCII digit character, false otherwise.
 */
static inline int isdigit(int c)
{
	return c >= '0' && c <= '9';
}

/**
 * Returns true if `c` is an ASCII lowercase character, false otherwise.
 */
static inline int islower(int c)
{
	return (c >= 'a') && (c <= 'z');
}

/**
 * Returns true if `c` is an ASCII printable character, false otherwise.
 */
static inline int isprint(int c)
{
	return c >= '\x20' && c <= '\x7e';
}

/**
 * Returns true if `c` is an ASCII whitespace character, false otherwise.
 */
static inline int isspace(int c)
{
	switch (c)
	{
		default:
			return 0;
		case '\t':
		case '\n':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
			return 1;
	}
}

/**
 * Returns true if `c` is an ASCII uppercase character, false otherwise.
 */
static inline int isupper(int c)
{
	return (c >= 'A') && (c <= 'Z');
}

/**
 * Returns true if `c` is an ASCII character that is part of the Latin
 * alphabet, false otherwise.
 */
static inline int isalpha(int c)
{
	return islower(c) || isupper(c);
}

/**
 * Returns true if `c` is an ASCII hexadecimal digit (0-f), false otherwise.
 */
static inline int isxdigit(int c)
{
	return isdigit(c) || ((c >= 'A') && (c <= 'F')) ||
	       ((c >= 'a') && (c <= 'f'));
}

/**
 * Converts an uppercase ASCII character to a lowercase one, or returns the
 * original value if `c` is not an uppercase character.
 */
static inline int tolower(int c)
{
	return c + (isupper(c) ? c : 0);
}

/**
 * Converts a lowercase ASCII character to a uppercase one, or returns the
 * original value if `c` is not an lowercase character.
 */
static inline int toupper(int c)
{
	return c - (islower(c) ? c : 0);
}

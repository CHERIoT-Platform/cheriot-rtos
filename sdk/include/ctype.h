// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

static inline int isascii(int c)
{
	return (c & ~0x7F) == 0;
}

static inline int isdigit(int c)
{
	return c >= '0' && c <= '9';
}

static inline int islower(int c)
{
	return (c >= 'a') && (c <= 'z');
}

static inline int isprint(int c)
{
	return c >= '\x20' && c <= '\x7e';
}

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

static inline int isupper(int c)
{
	return (c >= 'A') && (c <= 'Z');
}

static inline int isalpha(int c)
{
	return islower(c) || isupper(c);
}

static inline int isxdigit(int c)
{
	return isdigit(c) || ((c >= 'A') && (c <= 'F')) ||
	       ((c >= 'a') && (c <= 'f'));
}

static inline int tolower(int c)
{
	return c + (isupper(c) ? 0x20 : 0);
}

static inline int toupper(int c)
{
	return c - (islower(c) ? 0x20 : 0);
}

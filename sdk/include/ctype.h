// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

static inline int isprint(int c)
{
	return c >= '\x20' && c <= '\x7e';
}

static inline int isdigit(int c)
{
	return c >= '0' && c <= '9';
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


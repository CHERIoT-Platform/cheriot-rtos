// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#define CHAR_BIT __CHAR_BIT__

#define SCHAR_MAX __SCHAR_MAX__
#define SCHAR_MIN (-SCHAR_MAX - 1)
#define UCHAR_MAX (SCHAR_MAX * 2U + 1U)

#ifdef __CHAR_SIGNED__
#	define CHAR_MIN 0
#	define CHAR_MAX UCHAR_MAX
#else
#	define CHAR_MIN SCHAR_MIN
#	define CHAR_MAX SCHAR_MAX
#endif

#define MB_LEN_MAX 1

#define SHRT_MAX __SHRT_MAX__
#define SHRT_MIN (-SHRT_MAX - 1)

#define USHRT_MAX (SHRT_MAX * 2U + 1U)

#define __UWIDTH_MAX_IMPL(x) __UINT##x##_MAX__
#define __UWIDTH_MAX(x) __UWIDTH_MAX_IMPL(x)

#define INT_MAX __INT_MAX__
#define INT_MIN (-INT_MAX - 1)
#define UINT_MAX __UWIDTH_MAX(__INT_WIDTH__)

#define LONG_MAX __LONG_MAX__
#define LONG_MIN (-LONG_MAX - 1)
#define ULONG_MAX __UWIDTH_MAX(__LONG_WIDTH__)

#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-LLONG_MAX - 1)
#define ULLONG_MAX __UWIDTH_MAX(__LLONG_WIDTH__)

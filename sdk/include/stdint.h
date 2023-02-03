// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#define CHAR_BIT 8

#define __constant_integer_suffix_impl(a, b) a##b
#define __constant_integer_suffix(a, b) __constant_integer_suffix_impl(a, b)

typedef __UINT8_TYPE__       uint8_t;
typedef __UINT_LEAST8_TYPE__ uint_least8_t;
typedef __UINT_FAST8_TYPE__  uint_fast8_t;
#define UINT8_C(x) __constant_integer_suffix(x, __UINT8_C_SUFFIX__)
#define UINT8_MAX __UINT8_MAX__

typedef __INT8_TYPE__       int8_t;
typedef __INT_LEAST8_TYPE__ int_least8_t;
typedef __INT_FAST8_TYPE__  int_fast8_t;
#define INT8_C(x) __constant_integer_suffix(x, __INT8_C_SUFFIX__)
#define INT8_MAX __INT8_MAX__
#define INT8_MIN ((-INT8_C(INT8_MAX)) - 1)

typedef __UINT16_TYPE__       uint16_t;
typedef __UINT_LEAST16_TYPE__ uint_least16_t;
typedef __UINT_FAST16_TYPE__  uint_fast16_t;
#define UINT16_C(x) __constant_integer_suffix(x, __UINT16_C_SUFFIX__)
#define UINT16_MAX __UINT16_MAX__

typedef __INT16_TYPE__       int16_t;
typedef __INT_LEAST16_TYPE__ int_least16_t;
typedef __INT_FAST16_TYPE__  int_fast16_t;
#define INT16_C(x) __constant_integer_suffix(x, __INT16_C_SUFFIX__)
#define INT16_MAX __INT16_MAX__
#define INT16_MIN ((-INT16_C(INT16_MAX)) - 1)

typedef __UINT32_TYPE__       uint32_t;
typedef __UINT_LEAST32_TYPE__ uint_least32_t;
typedef __UINT_FAST32_TYPE__  uint_fast32_t;
#define UINT32_C(x) __constant_integer_suffix(x, __UINT32_C_SUFFIX__)
#define UINT32_MAX __UINT32_MAX__

typedef __INT32_TYPE__       int32_t;
typedef __INT_LEAST32_TYPE__ int_least32_t;
typedef __INT_FAST32_TYPE__  int_fast32_t;
#define INT32_C(x) __constant_integer_suffix(x, __INT32_C_SUFFIX__)
#define INT32_MAX __INT32_MAX__
#define INT32_MIN ((-INT32_C(INT32_MAX)) - 1)

typedef __UINT64_TYPE__       uint64_t;
typedef __UINT_LEAST64_TYPE__ uint_least64_t;
typedef __UINT_FAST64_TYPE__  uint_fast64_t;
#define UINT64_C(x) __constant_integer_suffix(x, __UINT64_C_SUFFIX__)
#define UINT64_MAX __UINT64_MAX__

typedef __INT64_TYPE__       int64_t;
typedef __INT_LEAST64_TYPE__ int_least64_t;
typedef __INT_FAST64_TYPE__  int_fast64_t;
#define INT64_C(x) __constant_integer_suffix(x, __INT64_C_SUFFIX__)
#define INT64_MAX __INT64_MAX__
#define INT64_MIN ((-INT64_C(INT64_MAX)) - 1)

typedef __UINTMAX_TYPE__ uintmax_t;
#define UINTMAX_C(x) __constant_integer_suffix(x, __UINTMAX_C_SUFFIX__)
#define UINTMAX_MAX __UINTMAX_MAX__
typedef __INTMAX_TYPE__ intmax_t;
#define INTMAX_C(x) __constant_integer_suffix(x, __INTMAX_C_SUFFIX__)
#define INTMAX_MAX __INTMAX_MAX__
#define INTMAX_MIN ((-INTMAX_C(INTMAX_MAX)) - 1)

typedef __UINTPTR_TYPE__ uintptr_t;
#define UINTPTR_MAX __UINTPTR_MAX__
typedef __INTPTR_TYPE__ intptr_t;
#define INTPTR_MAX __INTPTR_MAX__
#define INTPTR_MIN ((-INTPTR_C(INTPTR_MAX)) - 1)

#define SIZE_MAX __SIZE_MAX__

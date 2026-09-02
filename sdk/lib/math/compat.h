#pragma once

#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming)
typedef float    float_t;
typedef float    __float_t;
typedef double   __double_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;

#define _BIG_ENDIAN __ORDER_BIG_ENDIAN__
#define BIG_ENDIAN __ORDER_BIG_ENDIAN__

#define _LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__

#define _BYTE_ORDER __BYTE_ORDER__
#define BYTE_ORDER __BYTE_ORDER__

#undef __CONCAT
#define __CONCAT1(x, y) x##y
#define __CONCAT(x, y) __CONCAT1(x, y)

#define isnan(x) __builtin_isnan(x)
// NOLINTEND(readability-identifier-naming)

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef _ASSERT_H_
#define _ASSERT_H_

#include <stdlib.h>

#ifdef NDEBUG
#	define assert(x) ((void)0)
#else
#	define assert(x) ((x) ? (void)0 : panic())
#endif

#endif // _ASSERT_H_

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#ifndef NULL
#	ifdef __cplusplus
#		define NULL nullptr
#	else
#		define NULL ((void *)0)
#	endif
#endif

typedef __SIZE_TYPE__    size_t;
typedef signed int       ssize_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
typedef __UINTPTR_TYPE__ maxalign_t;

/// CHERI C definition for an address-sized integer
typedef __PTRADDR_TYPE__ ptraddr_t;

/// Compatibility definition
typedef ptraddr_t vaddr_t;

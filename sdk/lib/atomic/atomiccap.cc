// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT
#include "atomic.hh"

/**
 * This file defines the helper functions that are required for atomic
 * operations on pointers.
 */

DECLARE_ATOMIC_LIBCALL(__atomic_compare_exchange_cap,
                       int,
                       void **,
                       void **,
                       void *,
                       int,
                       int);
DECLARE_ATOMIC_LIBCALL(__atomic_load_cap, void *const *, int)
DECLARE_ATOMIC_LIBCALL(__atomic_store_cap, void, void **, void *, int)
DECLARE_ATOMIC_LIBCALL(__atomic_exchange_cap, void *, void **, void *, int)

int __atomic_compare_exchange_cap(void **ptr,
                                  void **expected,
                                  void  *desired,
                                  int,
                                  int)
{
	if (__builtin_cheri_equal_exact(*ptr, *expected))
	{
		*ptr = desired;
		return 1;
	}
	*expected = *ptr;
	return 0;
}

void *__atomic_load_cap(void *const *ptr, int)
{
	return *ptr;
}

void __atomic_store_cap(void **ptr, void *value, int)
{
	*ptr = value;
}

void *__atomic_exchange_cap(void **ptr, void *value, int)
{
	void *tmp = *ptr;
	*ptr      = value;
	return tmp;
}

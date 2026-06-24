// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "atomic.hh"
#include "string.h"

/**
 * Atomically load a variable number of bytes from `src` and store them in
 * `dest`.
 */
void atomic_load(int size, const void *src, void *dest, int)
{
	memcpy(dest, src, size);
}

/**
 * Atomically load a variable number of bytes from `src` and store them in
 * `dest`.
 */
void atomic_store(int size, void *dest, const void *src, int)
{
	memcpy(dest, src, size);
}

/**
 * Atomically replace the value at `ptr` with the value at `val` and the return
 * the old value via `old`.  All pointers point to at least `size` bytes.
 */
void atomic_exchange(int size, void *ptr, void *old, const void *val, int)
{
	memcpy(old, ptr, size);
	memcpy(ptr, val, size);
}

/**
 * Attempt to atomically replace `size` bytes pointed to by `ptr` with those at
 * `desired`, if and only if they currently contain the same pattern pointed to
 * by `expected`.  Returns 1 on success, 0 on failure.  In case of failure, the
 * current object stored at `ptr` will be copied over `expected`.
 */
int atomic_compare_exchange(int         size,
                            void       *ptr,
                            void       *expected,
                            const void *desired,
                            int,
                            int)
{
	if (memcmp(ptr, expected, size) == 0)
	{
		memcpy(ptr, desired, size);
		return 1;
	}
	memcpy(expected, ptr, size);
	return 0;
}

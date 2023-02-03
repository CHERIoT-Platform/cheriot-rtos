// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "atomic.hh"
#include "string.h"

/**
 * The variable-sized libcalls have the same names as builtins and clang
 * doesn't let us define functions with the same name as builtins, so we define
 * them without the `__` prefix and expose their assembly symbol as the expected
 * name.
 */
#define DECLARE_ATOMIC_LIBCALL_ALIAS(name, ret, ...)                           \
	[[cheri::interrupt_state(disabled)]] __cheri_libcall ret name(             \
	  __VA_ARGS__) asm("__" #name);

/**
 * Atomically load a variable number of bytes from `src` and store them in
 * `dest`.
 */
DECLARE_ATOMIC_LIBCALL_ALIAS(atomic_load, void, int, const void *, void *, int)
void atomic_load(int size, const void *src, void *dest, int)
{
	memcpy(dest, src, size);
}

/**
 * Atomically load a variable number of bytes from `src` and store them in
 * `dest`.
 */
DECLARE_ATOMIC_LIBCALL_ALIAS(atomic_store, void, int, void *, const void *, int)
void atomic_store(int size, void *dest, const void *src, int)
{
	memcpy(dest, src, size);
}

/**
 * Atomically replace the value at `ptr` with the value at `val` and the return
 * the old value via `old`.  All pointers point to at least `size` bytes.
 */
DECLARE_ATOMIC_LIBCALL_ALIAS(atomic_exchange,
                             void,
                             int,
                             void *,
                             void *,
                             const void *,
                             int)
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
DECLARE_ATOMIC_LIBCALL_ALIAS(atomic_compare_exchange,
                             int,
                             int,
                             void *,
                             void *,
                             const void *,
                             int,
                             int)
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

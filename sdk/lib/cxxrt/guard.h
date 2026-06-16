// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <stdint.h>

/**
 * The helper functions need to expose an unmangled name because the compiler
 * inserts calls to them.  Declare them using the asm label extension.
 */
#define DECLARE_LIBCALL(name, ret, ...)                                        \
	__cheri_libcall ret name(__VA_ARGS__) asm(#name);

// NOLINTBEGIN(readability-identifier-naming)
DECLARE_LIBCALL(__cxa_guard_acquire, int, uint64_t *)
DECLARE_LIBCALL(__cxa_guard_release, void, uint64_t *)
DECLARE_LIBCALL(__cxa_atexit, int, void (*)(void *), void *, void *)
// NOLINTEND(readability-identifier-naming)

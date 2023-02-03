// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <timeout.h>

__BEGIN_DECLS
static inline void __dead2 panic()
{
	// Invalid instruction is guaranteed to trap.
	while (1)
	{
		__asm volatile("unimp");
	}
}

/**
 * Non-standard allocation API.  Allocates `size` bytes.  Blocking behaviour is
 * controlled by the `timeout` parameter.
 *
 * The non-blocking mode will return a successful allocation if one can be
 * created immediately, or `nullptr` otherwise.
 * The blocking versions of this may return `nullptr` if the timeout has expired
 * or if the allocation cannot be satisfied under any circumstances (for example
 * if `size` is larger than the total heap size).
 *
 * Memory returned from this interface is guaranteed to be zeroed.
 */
void *__cheri_compartment("alloc") heap_allocate(size_t size, Timeout *timeout);

/**
 * Non-standard allocation API.  Allocates `size` * `nmemb` bytes of memory,
 * checking for arithmetic overflow.  Blocking behaviour is controlled by the
 * `timeout` parameter:
 *
 *  - 0 indicates that this call may not block.
 *  - The maximum value of the type indicates that this may block indefinitely.
 *  - Any other value indicates that this may block for, at most, that many
 *    ticks.
 *
 * The non-blocking mode will return a successful allocation if one can be
 * created immediately, or `nullptr` otherwise.
 * The blocking versions of this may return `nullptr` if the timeout has expired
 * or if the allocation cannot be satisfied under any circumstances (for example
 * if `nmemb` * `size` is larger than the total heap size, or if `nmemb` *
 * `size` overflows).
 *
 * Memory returned from this interface is guaranteed to be zeroed.
 */
void *__cheri_compartment("alloc")
  heap_allocate_array(size_t nmemb, size_t size, Timeout *timeout);

/**
 * Free a heap allocation.
 *
 * Returns 0 on success, or `-EINVAL` if `ptr` is not a valid pointer to the
 * start of a live heap allocation.
 */
int __cheri_compartment("alloc") heap_free(void *ptr);

static inline void __dead2 abort()
{
	panic();
}

static inline void *malloc(size_t size)
{
	Timeout t = {0, UnlimitedTimeout};
	return heap_allocate(size, &t);
}
static inline void *calloc(size_t nmemb, size_t size)
{
	Timeout t = {0, UnlimitedTimeout};
	return heap_allocate_array(nmemb, size, &t);
}
static inline int free(void *ptr)
{
	return heap_free(ptr);
}

static inline void yield(void)
{
	__asm volatile("ecall");
}
__END_DECLS

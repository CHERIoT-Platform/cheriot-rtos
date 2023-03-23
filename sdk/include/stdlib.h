// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <compartment-macros.h>
#include <stddef.h>
#include <stdint.h>
#include <timeout.h>

/**
 * `MALLOC_QUOTA` sets the quota for the current compartment for use with
 * malloc and free.  This defaults to 4 KiB.
 */
#ifndef MALLOC_QUOTA
#	define MALLOC_QUOTA 4096
#endif

/**
 * The public view of state represented by a capability that is used to
 * authorise heap allocations.  This should be used only when creating a new
 * heap.
 */
struct AllocatorCapabilityState
{
	/// The number of bytes that the capability will permit to be allocated.
	size_t quota;
	/// Reserved space for internal use.
	size_t unused;
	/// Reserved space for internal use.
	uintptr_t reserved[2];
};

struct SObjStruct;

/**
 * Helper macro to forward declare an allocator capability.
 */
#define DECLARE_ALLOCATOR_CAPABILITY(name)                                     \
	DECLARE_STATIC_SEALED_VALUE(                                               \
	  struct AllocatorCapabilityState, allocator, MallocKey, name);

/**
 * Helper macro to define an allocator capability authorising the specified
 * quota.
 */
#define DEFINE_ALLOCATOR_CAPABILITY(name, quota)                               \
	DEFINE_STATIC_SEALED_VALUE(struct AllocatorCapabilityState,                \
	                           alloc,                                          \
	                           MallocKey,                                      \
	                           name,                                           \
	                           (quota),                                        \
	                           0,                                              \
	                           {0, 0});

#ifndef CHERIOT_NO_AMBIENT_MALLOC
/**
 * Declare a default capability for use with malloc-style APIs.  Compartments
 * that are not permitted to allocate memory (or which wish to use explicit
 * heaps) can define `CHERIOT_NO_AMBIENT_MALLOC` to disable this.
 */
DECLARE_ALLOCATOR_CAPABILITY(__default_malloc_capability)
#	ifndef CUSTOM_DEFAULT_MALLOC_CAPARBILITY
/**
 * Define the capability for use with malloc-style APIs. In C code, this may be
 * defined in precisely on compilation unit per compartment.  Others should
 * define `CUSTOM_DEFAULT_MALLOC_CAPARBILITY` to avoid the definition.
 */
DEFINE_ALLOCATOR_CAPABILITY(__default_malloc_capability, MALLOC_QUOTA)
#	endif
#endif

/**
 * Helper macro to look up the default malloc capability.
 */
#define MALLOC_CAPABILITY STATIC_SEALED_VALUE(__default_malloc_capability)

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
void *__cheri_compartment("alloc")
  heap_allocate(Timeout           *timeout,
                struct SObjStruct *heapCapability,
                size_t             size);

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
  heap_allocate_array(Timeout           *timeout,
                      struct SObjStruct *heapCapability,
                      size_t             nmemb,
                      size_t             size);

/**
 * Free a heap allocation.
 *
 * Returns 0 on success, or `-EINVAL` if `ptr` is not a valid pointer to the
 * start of a live heap allocation.
 */
int __cheri_compartment("alloc")
  heap_free(struct SObjStruct *heapCapability, void *ptr);

/**
 * Returns 0 if the allocation can be freed with the given capability, a
 * negated errno value otherwise.
 */
int __cheri_compartment("alloc")
  heap_can_free(struct SObjStruct *heapCapability, void *ptr);

/**
 * Returns the space available in the given quota. This will return -1 if
 * `heapCapability` is not valid.
 */
size_t __cheri_compartment("alloc")
  heap_quota_remaining(struct SObjStruct *heapCapability);

/**
 * Block until the quarantine is empty.
 *
 * This should be used only in testing, to place the system in a quiesced
 * state.  It can block indefinitely if another thread is allocating and
 * freeing memory while this runs.
 */
void __cheri_compartment("alloc") heap_quarantine_empty(void);

static inline void __dead2 abort()
{
	panic();
}

#ifndef CHERIOT_NO_AMBIENT_MALLOC
static inline void *malloc(size_t size)
{
	Timeout t = {0, UnlimitedTimeout};
	return heap_allocate(&t, MALLOC_CAPABILITY, size);
}
static inline void *calloc(size_t nmemb, size_t size)
{
	Timeout t = {0, UnlimitedTimeout};
	return heap_allocate_array(&t, MALLOC_CAPABILITY, nmemb, size);
}
static inline int free(void *ptr)
{
	return heap_free(MALLOC_CAPABILITY, ptr);
}
#endif

static inline void yield(void)
{
	__asm volatile("ecall");
}
__END_DECLS

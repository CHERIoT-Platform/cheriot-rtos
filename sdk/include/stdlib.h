// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <compartment-macros.h>
#include <riscvreg.h>
#include <stddef.h>
#include <stdint.h>
#include <timeout.h>

/**
 * The SDK allocator implementation works in terms of "chunks" and has minimum
 * size requirements for these.  This is occasionally visible to its clients,
 * as documented on interface functions below.
 */
static const size_t CHERIOTHeapMinChunkSize = 16;

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

/**
 * Helper macro to define an allocator capability without a separate
 * declaration.
 */
#define DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(name, quota)                   \
	DECLARE_ALLOCATOR_CAPABILITY(name);                                        \
	DEFINE_ALLOCATOR_CAPABILITY(name, quota)

#ifndef CHERIOT_NO_AMBIENT_MALLOC
/**
 * Declare a default capability for use with malloc-style APIs.  Compartments
 * that are not permitted to allocate memory (or which wish to use explicit
 * heaps) can define `CHERIOT_NO_AMBIENT_MALLOC` to disable this.
 */
DECLARE_ALLOCATOR_CAPABILITY(__default_malloc_capability)
#	ifndef CHERIOT_CUSTOM_DEFAULT_MALLOC_CAPABILITY
/**
 * Define the capability for use with malloc-style APIs. In C code, this may be
 * defined in precisely on compilation unit per compartment.  Others should
 * define `CHERIOT_CUSTOM_DEFAULT_MALLOC_CAPABILITY` to avoid the definition.
 */
DEFINE_ALLOCATOR_CAPABILITY(__default_malloc_capability, MALLOC_QUOTA)
#	endif
#endif

/**
 * Helper macro to look up the default malloc capability.
 */
#define MALLOC_CAPABILITY STATIC_SEALED_VALUE(__default_malloc_capability)

#ifndef MALLOC_WAIT_TICKS
/**
 * Define how long a call to `malloc` and `calloc` can block to fulfil an
 * allocation. Regardless of this value, `malloc` and `calloc` will only ever
 * block to wait for the quarantine to be processed. This means that, even with
 * a non-zero value of `MALLOC_WAIT_TICKS`, `malloc` would immediately return
 * if the heap or the quota is exhausted.
 */
#	define MALLOC_WAIT_TICKS 30
#endif

__BEGIN_DECLS
static inline void __dead2 panic()
{
	// Invalid instruction is guaranteed to trap.
	while (1)
	{
		__asm volatile("unimp");
	}
}

enum [[clang::flag_enum]] AllocateWaitFlags{
  /**
   * Non-blocking mode. This is equivalent to passing a timeout with no time
   * remaining.
   */
  AllocateWaitNone = 0,
  /**
   * If there is enough memory in the quarantine to fulfil the allocation, wait
   * for the revoker to free objects from the quarantine.
   */
  AllocateWaitRevocationNeeded = (1 << 0),
  /**
   * If the quota of the passed heap capability is exceeded, wait for other
   * threads to free allocations.
   */
  AllocateWaitQuotaExceeded = (1 << 1),
  /**
   * If the heap memory is exhausted, wait for any other thread of the system
   * to free allocations.
   */
  AllocateWaitHeapFull = (1 << 2),
  /**
   * Block on any of the above reasons. This is the default behavior.
   */
  AllocateWaitAny = (AllocateWaitRevocationNeeded | AllocateWaitQuotaExceeded |
                     AllocateWaitHeapFull),
};

/**
 * Non-standard allocation API.  Allocates `size` bytes.
 *
 * The `heapCapability` quota object must have remaining capacity sufficient
 * for the requested `size` as well as any padding required by the CHERIoT
 * capability encoding (see its ISA document for details) and any additional
 * space required by the allocator's internal layout, which may be up to
 * `CHERIOTHeapMinChunkSize` bytes.  Not all of these padding bytes may be
 * available for use via the returned capability.
 *
 * Blocking behaviour is controlled by the `flags` and the `timeout` parameters.
 * Specifically, the `flags` parameter defines on which conditions to wait, and
 * the `timeout` parameter how long to wait.
 *
 * The non-blocking mode (`AllocateWaitNone`, or `timeout` with no time
 * remaining) will return a successful allocation if one can be created
 * immediately, or `nullptr` otherwise.
 *
 * The blocking modes may return `nullptr` if the condition to wait is not
 * fulfiled, if the timeout has expired, or if the allocation cannot be
 * satisfied under any circumstances (for example if `size` is larger than the
 * total heap size).
 *
 * This means that calling this with `AllocateWaitAny` and `UnlimitedTimeout`
 * will only ever return `nullptr` if the allocation cannot be satisfied under
 * any circumstances.
 *
 * In both blocking and non-blocking cases, `-ENOTENOUGHSTACK` may be returned
 * if the stack is insufficiently large to safely run the function. This means
 * that the return value of `heap_allocate` should be checked for the validity
 * of the tag bit *and not* nullptr.
 *
 * Memory returned from this interface is guaranteed to be zeroed.
 */
void *__cheri_compartment("alloc")
  heap_allocate(Timeout           *timeout,
                struct SObjStruct *heapCapability,
                size_t             size,
                uint32_t flags     __if_cxx(= AllocateWaitAny));

/**
 * Non-standard allocation API.  Allocates `size` * `nmemb` bytes of memory,
 * checking for arithmetic overflow. Similarly to `heap_allocate`, blocking
 * behaviour is controlled by the `flags` and the `timeout` parameters.
 *
 * See `heap_allocate` for more information on the padding and blocking
 * behavior.  One difference between this and `heap_allocate` is the definition
 * of when the allocation cannot be satisfied under any circumstances, which is
 * here if `nmemb` * `size` is larger than the total heap size, or if `nmemb` *
 * `size` overflows.
 *
 * Similarly to `heap_allocate`, `-ENOTENOUGHSTACK` may be returned if the
 * stack is insufficiently large to run the function. See `heap_allocate`.
 *
 * Memory returned from this interface is guaranteed to be zeroed.
 */
void *__cheri_compartment("alloc")
  heap_allocate_array(Timeout           *timeout,
                      struct SObjStruct *heapCapability,
                      size_t             nmemb,
                      size_t             size,
                      uint32_t flags     __if_cxx(= AllocateWaitAny));

/**
 * Add a claim to an allocation.  The object will be counted against the quota
 * provided by the first argument until a corresponding call to `heap_free`.
 * Note that this can be used with interior pointers.
 *
 * This will return the size of the allocation claimed on success (which may be
 * larger than the size requested in the original `heap_allocate` call; see its
 * documentation for more information), 0 on error (if `heapCapability` or
 * `pointer` is not valid, etc.), or `-ENOTENOUGHSTACK` if the stack is
 * insufficiently large to run the function.
 */
ssize_t __cheri_compartment("alloc")
  heap_claim(struct SObjStruct *heapCapability, void *pointer);

/**
 * Interface to the fast claims mechanism.  This claims two pointers using the
 * hazard-pointer-inspired lightweight claims mechanism.  If this function
 * returns zero then the heap pointers are guaranteed not to become invalid
 * until either the next cross-compartment call or the next call to this
 * function.
 *
 * A null pointer can be used as a not-present value.  This function will treat
 * operations on null pointers as unconditionally successful.  It returns
 * `-ETIMEDOUT` if it failed to claim before the timeout expired, or `EINVAL`
 * if one or more of the arguments is neither null nor a valid pointer at the
 * end.
 *
 * In the case of failure, neither pointer will have been claimed.
 *
 * This function is provided by the compartment_helpers library, which must be
 * linked for it to be available.
 */
int __cheri_libcall heap_claim_fast(Timeout         *timeout,
                                    const void      *ptr,
                                    const void *ptr2 __if_cxx(= nullptr));

/**
 * Free a heap allocation.
 *
 * Returns 0 on success, `-EINVAL` if `ptr` is not a valid pointer to the start
 * of a live heap allocation, or `-ENOTENOUGHSTACK` if the stack size is
 * insufficiently large to safely run the function.
 */
int __cheri_compartment("alloc")
  heap_free(struct SObjStruct *heapCapability, void *ptr);

/**
 * Free all allocations owned by this capability.
 *
 * Returns the number of bytes freed, `-EPERM` if this is not a valid heap
 * capability, or `-ENOTENOUGHSTACK` if the stack size is insufficiently large
 * to safely run the function.
 */
ssize_t __cheri_compartment("alloc")
  heap_free_all(struct SObjStruct *heapCapability);

/**
 * Returns 0 if the allocation can be freed with the given capability, a
 * negated errno value otherwise.
 */
int __cheri_compartment("alloc")
  heap_can_free(struct SObjStruct *heapCapability, void *ptr);

/**
 * Returns the space available in the given quota. This will return -1 if
 * `heapCapability` is not valid or if the stack is insufficient to run the
 * function.
 */
ssize_t __cheri_compartment("alloc")
  heap_quota_remaining(struct SObjStruct *heapCapability);

/**
 * Block until the quarantine is empty.
 *
 * This should be used only in testing, to place the system in a quiesced
 * state.  It can block indefinitely if another thread is allocating and
 * freeing memory while this runs.
 *
 * Returns 0 on success, a compartment invocation failure indication
 * (-ENOTENOUGHSTACK, -ENOTENOUGHTRUSTEDSTACK) if it cannot be invoked, or
 * possibly -ECOMPARTMENTFAIL if the allocator compartment is damaged.
 */
int __cheri_compartment("alloc") heap_quarantine_empty(void);

/**
 * Returns true if `object` points to a valid heap address, false otherwise.
 * Note that this does *not* check that this is a valid pointer.  This should
 * be used in conjunction with check_pointer to check validity.  The principle
 * use of this function is checking whether an object needs to be claimed.  If
 * this returns false but the pointer has global permission, it must be a
 * global and so does not need to be claimed.  If the pointer lacks global
 * permission then it cannot be claimed, but if this function returns false
 * then it is guaranteed not to go away for the duration of the call.
 */
__if_c(static) inline _Bool heap_address_is_valid(const void *object)
{
	ptraddr_t heap_start = LA_ABS(__export_mem_heap);
	ptraddr_t heap_end   = LA_ABS(__export_mem_heap_end);
	// The heap allocator has the only capability to the heap region.  Any
	// capability is either (transitively) derived from the heap capability or
	// derived from something else and so it is sufficient to check that the
	// base is within the range of the heap.  Anything derived from a non-heap
	// capability must have a base outside of that range.
	ptraddr_t address = __builtin_cheri_base_get(object);
	return (address >= heap_start) && (address < heap_end);
}

/**
 * Dump a textual rendering of the heap's structure to the debug console.
 *
 * If the RTOS is not built with --allocator-rendering=y, this is a no-op.
 *
 * Returns zero on success, non-zero on error (e.g. compartment call failure).
 */
int __cheri_compartment("alloc") heap_render();

static inline void __dead2 abort()
{
	panic();
}

#ifndef CHERIOT_NO_AMBIENT_MALLOC
static inline void *malloc(size_t size)
{
	Timeout t = {0, MALLOC_WAIT_TICKS};
	void   *ptr =
	  heap_allocate(&t, MALLOC_CAPABILITY, size, AllocateWaitRevocationNeeded);
	if (!__builtin_cheri_tag_get(ptr))
	{
		ptr = NULL;
	}
	return ptr;
}
static inline void *calloc(size_t nmemb, size_t size)
{
	Timeout t   = {0, MALLOC_WAIT_TICKS};
	void   *ptr = heap_allocate_array(
	    &t, MALLOC_CAPABILITY, nmemb, size, AllocateWaitRevocationNeeded);
	if (!__builtin_cheri_tag_get(ptr))
	{
		ptr = NULL;
	}
	return ptr;
}
static inline int free(void *ptr)
{
	return heap_free(MALLOC_CAPABILITY, ptr);
}
#endif

size_t __cheri_compartment("alloc") heap_available(void);

static inline void yield(void)
{
	__asm volatile("ecall" ::: "memory");
}
__END_DECLS

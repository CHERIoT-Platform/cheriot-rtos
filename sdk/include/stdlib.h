// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <__cheri_sealed.h>
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

/**
 * Type for allocator capabilities.
 */
typedef CHERI_SEALED(struct AllocatorCapabilityState *) AllocatorCapability;

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
	                           allocator,                                      \
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

/**
 * Helper macro to look up the default malloc capability.
 */
#	define MALLOC_CAPABILITY STATIC_SEALED_VALUE(__default_malloc_capability)
#endif

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

enum [[clang::flag_enum]] AllocateWaitFlags
{
	/**
	 * Non-blocking mode. This is equivalent to passing a timeout with no time
	 * remaining.
	 */
	AllocateWaitNone = 0,
	/**
	 * If there is enough memory in the quarantine to fulfil the allocation,
	 * wait for the revoker to free objects from the quarantine.
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
	AllocateWaitAny = (AllocateWaitRevocationNeeded |
	                   AllocateWaitQuotaExceeded | AllocateWaitHeapFull),
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
 * Returns `-EINVAL` if the provided timeout is invalid, and `-EPERM` if the
 * heap capability does not have permission to perform this allocation.
 *
 * The non-blocking mode (`AllocateWaitNone`, or `timeout` with no time
 * remaining) will return a successful allocation if one can be created
 * immediately, `-ENOMEM` for heap full or out-of-quota, `-EAGAIN` if
 * there is enough memory to satisfy the allocation in quaratine, indicating
 * that the operation could succeed if retried, or `-EINVAL` if the
 * allocation cannot succeed under any circumstances.
 *
 * The blocking modes may return error codes if:
 * - `-EAGAIN`: Revocation needed and flags specify no wait on this condition.
 * - `-ENOMEM`: The heap is full or provided quota is exceeded, and flags
 *   specify no wait on this condition
 * - `-ETIMEDOUT`: The timeout has expired while waiting to acquire the
 *   allocator lock or while retrying the allocation.
 * - `-EINVAL`: The allocation cannot be satisfied under any circumstances,
 *   for example if `size` is larger than the total heap size.
 *
 * In both blocking and non-blocking cases, `-ENOTENOUGHSTACK` may be returned
 * if the stack is insufficiently large to safely run the function.
 *
 * The return value of `heap_allocate` should be checked for the validity
 * of the tag bit *and not* simply compared against `nullptr`.
 *
 * Memory returned from this interface is guaranteed to be zeroed.
 */
void *__cheri_compartment("allocator")
  heap_allocate(Timeout            *timeout,
                AllocatorCapability heapCapability,
                size_t              size,
                uint32_t flags      __if_cxx(= AllocateWaitAny));

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
 * Returns `-EINVAL` on such an overflow, or when the provided timeout pointer
 * is invalid. Returns `-EPERM` if `heapCapability` does not have permission to
 * perform this allocation.
 *
 * Similarly to `heap_allocate`, `-ENOTENOUGHSTACK` may be returned if the
 * stack is insufficiently large to run the function. See `heap_allocate` for
 * other potential return values.
 *
 * Memory returned from this interface is guaranteed to be zeroed.
 */
void *__cheri_compartment("allocator")
  heap_allocate_array(Timeout            *timeout,
                      AllocatorCapability heapCapability,
                      size_t              nmemb,
                      size_t              size,
                      uint32_t flags      __if_cxx(= AllocateWaitAny));

/**
 * Free a heap allocation or release a claim made by `heap_claim`.
 *
 * To free an allocation `ptr` must be a capability to the entire allocation
 * with the same bounds as returned by `heap_allocate`. Claims, however, may be
 * released using a capability with reduced bounds, just as we allow
 * `heap_claim` with such a capability. Note that we actually free / release the
 * allocation associated with the *base* of `ptr`, not the address. This is
 * because the base is guaranteed to be within the bounds of the original
 * allocation due to capability monotonicity, unlike the address (which is
 * ignored)[1].
 *
 * Returns:
 *  - 0 on success
 *  - `-EINVAL` if `heapCapability` is invalid or if `ptr` is untagged, is
 *     sealed, or is not a heap pointer.
 *  - `-EPERM` if `ptr` is not owned or claimed by `heapCapability`, or if the
 *     bounds do not correspond to the whole allocation and there is no
 *     outstanding claim to release.
 *  - `-ENOTENOUGHSTACK` if the stack size is insufficiently large to safely run
 *     the function.
 *
 * [1]: Technically it is possible to derive a tagged, zero-length capability
 *     with the base equal to the top of an allocation (i.e. just past the end),
 *     but such capabilities are liable to have their tag bit cleared if stored
 *     to memory due to the allocator always setting the revocation bit on the
 *     header of the next allocation. Attempting to free such a pointer will
 *     result in -EINVAL.
 */
int __cheri_compartment("allocator")
  heap_free(AllocatorCapability heapCapability, void *ptr);

/**
 * Returns true if `object` points to a valid heap address, false otherwise.
 * Note that this does *not* check that this is a valid pointer.  This should
 * be used in conjunction with `check_pointer` to check validity.  The
 * principle use of this function is checking whether an object needs to be
 * claimed.  If this returns false but the pointer has global permission, it
 * must be a global and so does not need to be claimed.  If the pointer lacks
 * global permission then it cannot be claimed, but if this function returns
 * false then it is guaranteed not to go away for the duration of the call.
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

static inline void yield(void)
{
	__asm volatile("ecall" ::: "memory");
}

/**
 * Convert an ASCII string into a signed long.
 *
 * This function, and indeed the CHERIoT-RTOS in general, has no notion of
 * locale.
 *
 * While prototyped here, it is available as part of a dedicated 'strtol'
 * library, which may be omitted from firmware builds if the implementation is
 * not required.
 */
long __cheri_libcall strtol(const char *nptr, char **endptr, int base);

/**
 * Convert an ASCII string into an unsigned long.
 *
 * This function, and indeed the CHERIoT-RTOS in general, has no notion of
 * locale.
 *
 * While prototyped here, it is available as part of a dedicated 'strtol'
 * library, which may be omitted from firmware builds if the implementation is
 * not required.
 */
unsigned long __cheri_libcall strtoul(const char *nptr,
                                      char      **endptr,
                                      int         base);

__END_DECLS

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <riscvreg.h>
#include <stddef.h>
#include <stdint.h>
#include <timeout.h>
#include <token.h>

// Forward declarations from stdlib.h
struct AllocatorCapabilityState;
typedef CHERI_SEALED(struct AllocatorCapabilityState *) AllocatorCapability;

__BEGIN_DECLS

/**
 * Permission bits for allocator capabilities.
 * Permission to free is implicit, and cannot be restricted.
 */
enum [[clang::flag_enum]] AllocatorPermission
{
	/**
	 * This allocator capability is fully de-permissioned. Note that this
	 * capability can still be used to free.
	 */
	AllocatorPermitNone = 0,
	/**
	 * This allocator capability may be used to perform allocations and make
	 * claims using its quota.
	 */
	AllocatorPermitAllocate = (1 << 0),
	/**
	 * This allocator capability may be used to free all allocations made
	 * using its quota via `heap_free_all`.
	 */
	AllocatorPermitFreeAll = (1 << 1),
	/**
	 * Unused permissions bit.
	 */
	AllocatorPermitUnused = (1 << 2),
	/**
	 * This allocator capability is fully permissioned. It can be used to
	 * allocate and free all.
	 */
	AllocatorPermitFull = (AllocatorPermitAllocate | AllocatorPermitFreeAll |
	                       AllocatorPermitUnused),
};

/**
 * Returns the permissions held by this allocator capability. This is a
 * bitmask of `AllocatorPermission` values.
 */
static inline int allocator_permissions(AllocatorCapability heapCapability)
{
	return token_permissions_get(heapCapability);
}

/**
 * Returns a copy of `heapCapability` with a subset of permissions. The
 * `permissions` argument is a bitmask of `AllocatorPermission` values. The
 * returned allocator capability has only the permissions that are both already
 * present on `heapCapability` and enumerated in `permissions`.
 */
static inline AllocatorCapability
allocator_permissions_and(AllocatorCapability heapCapability, int permissions)
{
	return token_permissions_and(heapCapability, permissions);
}

/**
 * Add a claim to an allocation.  The object will be counted against the quota
 * provided by the first argument until a corresponding call to `heap_free`.
 * Note that this can be used with interior pointers.
 *
 * This will return the size of the allocation claimed on success (which may be
 * larger than the size requested in the original `heap_allocate` call; see its
 * documentation for more information).
 *
 * Returns `-EPERM` if `heapCapability` does not have permission to perform this
 * claim, `-ENOMEM` if the provided quota is too small to accomodate the claim,
 * and `-EINVAL` if `pointer` is not a valid pointer into a live heap
 * allocation.
 *
 * Similarly to `heap_allocate`, `-ENOTENOUGHSTACK` may be returned if the
 * stack is insufficiently large to run the function. See `heap_allocate`.
 */
ssize_t __cheri_compartment("allocator")
  heap_claim(AllocatorCapability heapCapability, void *pointer);

/**
 * Interface to the ephemeral claims mechanism.  This claims two pointers using
 * the hazard-pointer-inspired lightweight claims mechanism.  If this function
 * returns zero then the heap pointers are guaranteed not to become invalid
 * until either the next cross-compartment call or the next call to this
 * function.
 *
 * A null pointer can be used as a not-present value.  This function will treat
 * operations on null pointers as unconditionally successful.  It returns
 * `-ETIMEDOUT` if it failed to claim before the timeout expired, or `-EINVAL`
 * if one or more of the arguments is neither null nor a valid pointer at the
 * end.
 *
 * In the case of failure, neither pointer will have been claimed.
 *
 * This function is provided by the `compartment_helpers` library, which must be
 * linked for it to be available.
 */
int __cheri_libcall heap_claim_ephemeral(Timeout         *timeout,
                                         const void      *ptr,
                                         const void *ptr2 __if_cxx(= nullptr));

__attribute__((deprecated("heap_claim_fast was a bad name.  This function has "
                          "been renamed heap_claim_ephemeral")))
__always_inline static int
heap_claim_fast(Timeout         *timeout,
                const void      *ptr,
                const void *ptr2 __if_cxx(= nullptr))
{
	return heap_claim_ephemeral(timeout, ptr, ptr2);
}

/**
 * Free all allocations owned by this capability.
 *
 * Returns the number of bytes freed, `-EPERM` if this heap capability does not
 * have permission to perform this action, or `-ENOTENOUGHSTACK` if the stack
 * size is insufficiently large to safely run the function.
 */
ssize_t __cheri_compartment("allocator")
  heap_free_all(AllocatorCapability heapCapability);

/**
 * Returns 0 if the allocation can be freed with the given capability,
 * `-EPERM` if `heapCapability` is not valid or it cannot free the provided
 * allocation (such as when `heapCapability` does not own the allocation,
 * and holds no claim on it), and `-EINVAL` if `ptr` is not a valid pointer
 * into a live heap allocation.
 */
int __cheri_compartment("allocator")
  heap_can_free(AllocatorCapability heapCapability, void *ptr);

/**
 * Returns the space available in the given quota. This will return `-EPERM` if
 * `heapCapability` is not valid or `-ENOTENOUGHSTACK` if the stack is
 * insufficient to run the function.
 */
ssize_t __cheri_compartment("allocator")
  heap_quota_remaining(AllocatorCapability heapCapability);

/**
 * Try to empty the quarantine and defragment the heap.
 *
 * This will (finish and then) run a revocation sweep and try to empty the
 * quarantine.  In normal operation, the allocator will remove a small number of
 * allocations from quarantine on each allocation.  Allocations are not
 * coalesced until they are moved from quarantine, so this can cause
 * fragmentation.  If you have just freed a lot of memory (for example, after
 * resetting a compartment and calling `heap_free_all`), especially if you have
 * freed a lot of small allocations, then calling this function will likely
 * reduce fragmentation.
 *
 * Calling this function will ensure that all objects freed before the call are
 * out of quarantine (unless a timeout occurs).  Objects freed concurrently (by
 * another thread) may remain in quarantine, so this does not guarantee that
 * the quarantine is empty, only that everything freed by this thread is
 * removed from quarantine.
 *
 * Returns 0 on success, a compartment invocation failure indication
 * (`-ENOTENOUGHSTACK`, `-ENOTENOUGHTRUSTEDSTACK`) if it cannot be invoked, or
 * possibly `-ECOMPARTMENTFAIL` if the allocator compartment is damaged.
 *
 * Returns `-ETIMEDOUT` if the timeout expires or `-EINVAL` if the timeout is
 * not valid.
 */
__attribute__((overloadable)) int __cheri_compartment("allocator")
  heap_quarantine_flush(Timeout *timeout);

/**
 * Run `heap_quarantine_flush` with unlimited timeout.
 *
 * This is guaranteed to terminate (barring bugs), but, as with most
 * unlimited-timeout operation, should be confined to debug or test code.
 */
static int heap_quarantine_empty()
{
	Timeout t = {0, UnlimitedTimeout};
	return heap_quarantine_flush(&t);
}

/**
 * Dump a textual rendering of the heap's structure to the debug console.
 *
 * If the RTOS is not built with --allocator-rendering=y, this is a no-op.
 *
 * Returns zero on success, non-zero on error (e.g. compartment call failure).
 */
int __cheri_compartment("allocator") heap_render();

/**
 * Returns the total available space in the heap. This counts free space but
 * does not account for quota restrictions.
 */
size_t __cheri_compartment("allocator") heap_available(void);

__END_DECLS

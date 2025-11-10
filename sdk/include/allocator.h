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

__END_DECLS

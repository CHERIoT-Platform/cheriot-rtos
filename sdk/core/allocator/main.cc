// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "alloc.h"
#include "revoker.h"
#include "token.h"
#include <compartment.h>
#include <errno.h>
#include <futex.h>
#include <locks.hh>
#include <priv/riscv.h>
#include <riscvreg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>
#include <token.h>
#include <utils.h>

using namespace CHERI;

Revocation::Revoker revoker;
namespace
{
	/**
	 * Internal view of an allocator capability.
	 *
	 * TODO: For now, these are statically allocated. Eventually we will have
	 * some that are dynamically allocated. These should be ref counted so that
	 * we can avoid repeated validity checks.
	 */
	struct PrivateAllocatorCapabilityState
	{
		/// The remaining quota for this capability.
		size_t quota;
		/// A unique identifier for this pool.
		uint16_t identifier;
	};

	static_assert(sizeof(PrivateAllocatorCapabilityState) <=
	              sizeof(AllocatorCapabilityState));
	static_assert(alignof(PrivateAllocatorCapabilityState) <=
	              alignof(AllocatorCapabilityState));

	// the global memory space
	MState *gm;

	/**
	 * A global lock for the allocator.  This is acquired in public API
	 * functions, all internal functions should assume that it is held. If
	 * allocation fails for transient reasons then the lock will be dropped and
	 * reacquired over the yield.
	 */
	FlagLock lock;

	/**
	 * @brief Take a memory region and initialise a memory space for it. The
	 * MState structure will be placed at the beginning and the rest used as the
	 * real heap.
	 *
	 * @param tbase the capability to the region
	 * @param tsize size of the region
	 * @return pointer to the MState if can be initialised, nullptr otherwise.
	 */
	MState *mstate_init(Capability<void> tbase, size_t tsize)
	{
		if (!is_aligned(tbase) || !is_aligned(tsize))
		{
			return nullptr;
		}

		size_t msize = pad_request(sizeof(MState));
		/*
		 * Each memory space has the MState structure at the beginning, followed
		 * by at least enough space for a smallest chunk, followed by a fake
		 * malloc header at the very end.
		 *
		 * The memory used to initialise a memory space must have enough bytes,
		 * but not too big that overflows what the compressed header can
		 * support.
		 */
		if (tsize < msize + MinChunkSize + sizeof(MChunkHeader) ||
		    tsize > MaxChunkSize)
		{
			return nullptr;
		}

		Capability m{tbase.cast<MState>()};

		m.bounds()            = sizeof(*m);
		m->heapStart          = tbase;
		m->heapStart.bounds() = tsize;
		m->heapStart.address() += msize;
		m->init_bins();

		m->mspace_firstchunk_add(ds::pointer::offset<void>(tbase.get(), msize),
		                         tsize - msize);

		return m;
	}

	void check_gm()
	{
		if (gm == nullptr)
		{
			Capability heap = const_cast<void *>(MMIO_CAPABILITY(void, heap));

			revoker.init();
			gm = mstate_init(heap, heap.bounds());
			Debug::Assert(gm != nullptr, "gm should not be null");
		}
	}

	/**
	 * Futex value to allow a thread to wait for another thread to free an
	 * object.
	 */
	uint32_t freeFutex;

	/**
	 * Helper that returns true if the timeout value permits sleeping.
	 *
	 * This assumes that the pointer was checked with `check_pointer` earlier
	 * but defends against the case where the timeout object was freed by
	 * another thread while the thread using it slept.
	 */
	bool may_block(Timeout *timeout)
	{
		return (Capability{timeout}.is_valid() && timeout->may_block());
	}

	/**
	 * Malloc implementation.  Allocates `bytes` bytes of memory.  If `timeout`
	 * is greater than zero, may block for that many ticks.  If `timeout` is the
	 * maximum value permitted by the type, may block indefinitely.
	 *
	 * Memory is allocated against the quota in `capability`, which must be
	 * unsealed by the caller.
	 *
	 * The lock guard own the lock on entry. It will drop the lock if a
	 * transient error occurs and attempt to reacquire it. If the lock cannot be
	 * reacquired during the permitted timeout then this returns nullptr.
	 */
	void *malloc_internal(size_t                           bytes,
	                      LockGuard<decltype(lock)>      &&g,
	                      PrivateAllocatorCapabilityState *capability,
	                      Timeout                         *timeout)
	{
		check_gm();

		do
		{
			auto ret = gm->mspace_dispatch(
			  bytes, capability->quota, capability->identifier);
			if (std::holds_alternative<Capability<void>>(ret))
			{
				return std::get<Capability<void>>(ret);
			}
			// If the timeout is 0, fail now.
			if (!may_block(timeout))
			{
				return nullptr;
			}
			// If there is enough memory in the quarantine to fulfil this
			// allocation, try dequeing some things and retry.
			if (std::holds_alternative<
			      MState::AllocationFailureRevocationNeeded>(ret))
			{
				// If we are able to dequeue some objects from quarantine then
				// retry immediately, otherwise yield.
				//
				// It may take us several rounds through here to succeed or
				// discover fragmentation-induced futility, since we do not,
				// presently, consolidate chunks in quarantine and each chunk
				// requires individual attention to merge back into the free
				// pool (and consolidate with neighbors), and each round here
				// moves at most O(1) chunks out of quarantine.
				if (!gm->quarantine_dequeue())
				{
					Debug::log("Quarantine has enough memory to satisfy "
					           "allocation, kicking revoker");
					revoker.system_bg_revoker_kick();

					// Drop and reacquire the lock while yielding.
					g.unlock();
					// Sleep for a single tick.
					Timeout smallSleep{1};
					thread_sleep(&smallSleep);
					// It's possible that, while we slept, `*timeout` was
					// freed.  Check that the pointer is still valid so that we
					// don't fault if this happens. We are not holding the lock
					// at this point and so we must do the check with interrupts
					// disabled to protect against concurrent free.
					if (!with_interrupts_disabled([&]() {
						    if (Capability{timeout}.is_valid())
						    {
							    timeout->elapse(smallSleep.elapsed);
							    return g.try_lock(timeout);
						    }
						    return false;
					    }))
					{
						return nullptr;
					}
				}
				continue;
			}
			// If the heap is full, wait for someone to free an allocation and
			// then retry.
			if (std::holds_alternative<
			      MState::AllocationFailureDeallocationNeeded>(ret))
			{
				Debug::log("Not enough free space to handle {}-byte "
				           "allocation, sleeping",
				           bytes);
				// Drop the lock while yielding
				g.unlock();
				auto err = futex_timed_wait(timeout, &freeFutex, ++freeFutex);
				Debug::Assert(
				  err != -EINVAL,
				  "Invalid arguments to futex_timed_wait({}, {}, {})",
				  &freeFutex,
				  freeFutex,
				  timeout);
				// If we timed out, we don't need to reacquire the lock, just
				// exit
				if (err == -ETIMEDOUT)
				{
					return nullptr;
				}
				Debug::log("Woke from futex wake");
				if (!with_interrupts_disabled([&]() {
					    if (Capability{timeout}.is_valid())
					    {
						    return g.try_lock(timeout);
					    }
					    return false;
				    }))
				{
					return nullptr;
				}
				continue;
			}
			if (std::holds_alternative<MState::AllocationFailurePermanent>(ret))
			{
				return nullptr;
			}
		} while (may_block(timeout));
		return nullptr;
	}

	/**
	 * Unseal an allocator capability and return it.  Returns `nullptr` if this
	 * is not a heap capability.
	 */
	PrivateAllocatorCapabilityState *
	malloc_capability_unseal(SealedAllocation in)
	{
		Capability type = STATIC_SEALING_TYPE(MallocKey);
		Capability key{SEALING_CAP()};
		in.unseal(key);
		if (!in.is_valid() || (in->type != type.address()))
		{
			Debug::log("Invalid malloc capability {}", in);
			return nullptr;
		}
		auto *capability =
		  reinterpret_cast<PrivateAllocatorCapabilityState *>(&in->data);
		// Assign an identifier if this is the first time that we've seen this.
		if (capability->identifier == 0)
		{
			static uint32_t nextIdentifier = 1;
			if (nextIdentifier >
			    std::numeric_limits<decltype(capability->identifier)>::max())
			{
				return nullptr;
			}
			capability->identifier = nextIdentifier++;
		}
		return capability;
	}

	__noinline int
	heap_free_internal(SObj heapCapability, void *rawPointer, bool reallyFree)
	{
		auto *capability = malloc_capability_unseal(heapCapability);
		if (capability == nullptr)
		{
			Debug::log("Invalid heap capabilityL {}", heapCapability);
			return -EPERM;
		}
		Capability<void> mem{rawPointer};
		if (!mem.is_valid())
		{
			return 0;
		}
		// Use the default memory space.
		check_gm();
		if (!gm->is_free_cap_inbounds(mem))
		{
			return -EINVAL;
		}
		return gm->mspace_free(
		  mem, capability->quota, capability->identifier, reallyFree);
	}
} // namespace

size_t heap_quota_remaining(struct SObjStruct *heapCapability)
{
	LockGuard g{lock};
	auto     *cap = malloc_capability_unseal(heapCapability);
	if (cap == nullptr)
	{
		return -1;
	}
	return cap->quota;
}

void heap_quarantine_empty()
{
	LockGuard g{lock};
	while (gm->heapQuarantineSize > 0)
	{
		if (!gm->quarantine_dequeue())
		{
			revoker.system_bg_revoker_kick();
		}
		g.unlock();
		yield();
		g.lock();
	}
}

void *heap_allocate(Timeout *timeout, SObj heapCapability, size_t bytes)
{
	LockGuard g{lock};
	auto     *cap = malloc_capability_unseal(heapCapability);
	if (cap == nullptr)
	{
		return nullptr;
	}
	if (!check_pointer<PermissionSet{Permission::Load, Permission::Store}>(
	      timeout))
	{
		return nullptr;
	}
	// Use the default memory space.
	return malloc_internal(bytes, std::move(g), cap, timeout);
}

int heap_can_free(SObj heapCapability, void *rawPointer)
{
	LockGuard g{lock};
	return heap_free_internal(heapCapability, rawPointer, false);
}

int heap_free(SObj heapCapability, void *rawPointer)
{
	LockGuard g{lock};
	int       ret = heap_free_internal(heapCapability, rawPointer, true);
	if (ret != 0)
	{
		return ret;
	}

	// If there are any threads blocked allocating memory, wake them up.
	if (freeFutex > 0)
	{
		Debug::log("Some threads are blocking on allocations, waking them");
		freeFutex = 0;
		futex_wake(&freeFutex, -1);
	}

	return 0;
}

void *heap_allocate_array(Timeout *timeout,
                          SObj     heapCapability,
                          size_t   nElements,
                          size_t   elemSize)
{
	LockGuard g{lock};
	auto     *cap = malloc_capability_unseal(heapCapability);
	if (cap == nullptr)
	{
		return nullptr;
	}
	// Use the default memory space.
	size_t req;
	if (__builtin_mul_overflow(nElements, elemSize, &req))
	{
		return nullptr;
	}
	if (!check_pointer<PermissionSet{Permission::Load, Permission::Store}>(
	      timeout))
	{
		return nullptr;
	}
	return malloc_internal(req, std::move(g), cap, timeout);
}

namespace
{
	/**
	 * The next sealing type to allocate.  Sealing types are allocated from the
	 * top down.
	 */
	uint32_t nextSealingType = std::numeric_limits<uint32_t>::max();

	/**
	 * Returns the root for the software sealing key.
	 */
	__always_inline Capability<SKeyStruct> software_sealing_key()
	{
		SKeyStruct *ret;
		__asm("1:   "
		      " auipcc      %0, %%cheri_compartment_pccrel_hi(__sealingkey2)\n"
		      " clc         %0, %%cheri_compartment_pccrel_lo(1b)(%0)\n"
		      : "=C"(ret));
		return {ret};
	}

	/**
	 * Helper that unseals `in` if it is a valid sealed capability sealed with
	 * our hardware sealing key.  Returns the unsealed pointer, `nullptr` if it
	 * cannot be helped.
	 */
	SealedAllocation unseal_if_valid(SealedAllocation in)
	{
		// The input must be tagged and sealed with our type.
		// FIXME: At the moment the ISA is still shuffling types around, but
		// eventually we want to know the type statically and don't need dynamic
		// instructions.
		Capability key{SEALING_CAP()};
		in.unseal(key);
		return in.is_valid() ? in : SealedAllocation{nullptr};
	}

	/**
	 * Helper that allocates a sealed object and returns the sealed and
	 * unsealed capabilities to the object.  Requires that the sealing key have
	 * all of the permissions in `permissions`.
	 */
	std::pair<SObj, void *>
	  __noinline allocate_sealed_unsealed(Timeout      *timeout,
	                                      SObj          heapCapability,
	                                      SealingKey    key,
	                                      size_t        sz,
	                                      PermissionSet permissions)
	{
		if (!check_pointer<PermissionSet{Permission::Load, Permission::Store}>(
		      timeout))
		{
			return {nullptr, nullptr};
		}
		if (!permissions.can_derive_from(key.permissions()))
		{
			Debug::log(
			  "Operation requires {}, cannot derive from {}", permissions, key);
			return {nullptr, nullptr};
		}

		if (sz > 0xfe8 - ObjHdrSize)
		{
			Debug::log("Cannot allocate sealed object of {} bytes, too large",
			           sz);
			// TODO: Properly handle imprecision.
			return {nullptr, nullptr};
		}

		LockGuard g{lock};
		auto     *capability = malloc_capability_unseal(heapCapability);
		if (capability == nullptr)
		{
			return {nullptr, nullptr};
		}
		SealedAllocation obj{static_cast<SObj>(
		  malloc_internal(sz + ObjHdrSize, std::move(g), capability, timeout))};
		if (obj == nullptr)
		{
			Debug::log("Underlying allocation failed for sealed object");
			return {nullptr, nullptr};
		}

		obj->type   = key.address();
		auto sealed = obj;
		sealed.seal(SEALING_CAP());
		obj.address() += ObjHdrSize; // Exclude the header.
		obj.bounds() = obj.length() - ObjHdrSize;
		Debug::log("Allocated sealed {}, unsealed {}", sealed, obj);
		return {sealed, obj};
	}
} // namespace

SKey token_key_new()
{
	// This needs protecting against races but doesn't touch any other data
	// structures and so can have its own lock.
	static FlagLock tokenLock;
	LockGuard       g{tokenLock};
	auto            keyRoot = software_sealing_key();
	// For now, strip the user permissions.  We might want to use them for
	// permit-allocate and permit-free.
	keyRoot.permissions() &=
	  {Permission::Global, Permission::Seal, Permission::Unseal};
	// Allocate sealing types from the top.
	if (keyRoot.base() < nextSealingType - 1)
	{
		auto key      = keyRoot;
		key.address() = --nextSealingType;
		key.bounds()  = 1;
		Debug::log("Allocated sealing capability: {}", key);
		return key;
	}
	return nullptr;
}

SObj token_sealed_unsealed_alloc(Timeout *timeout,
                                 SObj     heapCapability,
                                 SKey     key,
                                 size_t   sz,
                                 void   **unsealed)
{
	auto [sealed, obj] = allocate_sealed_unsealed(
	  timeout, heapCapability, key, sz, {Permission::Seal, Permission::Unseal});
	{
		LockGuard g{lock};
		if (check_pointer<PermissionSet{
		      Permission::Store, Permission::LoadStoreCapability}>(unsealed))
		{
			*unsealed = obj;
			return sealed;
		}
	}
	heap_free(heapCapability, obj);
	return INVALID_SOBJ;
}

SObj token_sealed_alloc(Timeout *timeout,
                        SObj     heapCapability,
                        SKey     rawKey,
                        size_t   sz)
{
	return allocate_sealed_unsealed(
	         timeout, heapCapability, rawKey, sz, {Permission::Seal})
	  .first;
}

/**
 * Helper used to unseal a sealed object with a given key.  Performs all of the
 * relevant checks and returns nullptr if this is not a valid key and object
 * sealed with that key.
 */
__noinline static SealedAllocation unseal_internal(SKey rawKey, SObj obj)
{
	SealingKey key{rawKey};

	if (!key.permissions().contains(Permission::Unseal))
	{
		return nullptr;
	}

	auto unsealed = unseal_if_valid(obj);
	if (!unsealed)
	{
		return nullptr;
	}
	if (unsealed->type != key.address())
	{
		return nullptr;
	}

	return unsealed;
}

void *token_obj_unseal(SKey rawKey, SObj obj)
{
	LockGuard g{lock};
	auto      unsealed = unseal_internal(rawKey, obj);
	if (unsealed == nullptr)
	{
		return nullptr;
	}
	size_t newSize = unsealed.length() - ObjHdrSize;
	unsealed.address() += ObjHdrSize;
	unsealed.bounds() = newSize;
	return unsealed;
}

int token_obj_destroy(SObj heapCapability, SKey key, SObj object)
{
	void *unsealed;
	{
		LockGuard g{lock};
		unsealed = unseal_internal(key, object);
		if (unsealed == nullptr)
		{
			return -EINVAL;
		}
		// At this point, we drop and reacquire the lock. This is better for
		// code reuse and heap_free will catch races because it will check the
		// revocation state.
		// The key can't be revoked and so there is no race with the key going
		// away after the check.
	}
	return heap_free(heapCapability, unsealed);
}

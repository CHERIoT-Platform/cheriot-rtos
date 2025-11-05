// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "alloc.h"
#include "cheri.hh"
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
#include <utils.hh>

/**
 * The sealing key for dynamically allocated software-sealed objects.
 */
__attribute__((section(".sealing_key1"))) void *allocatorSealingKey;

/**
 * The root for the software sealing key.
 */
__attribute__((section(".sealing_key2"))) Capability<TokenKeyType>
                                          softwareSealingKey;

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
	FlagLockPriorityInherited lock;

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

		size_t hazardQuarantineSize =
		  Capability{
		    SHARED_OBJECT_WITH_PERMISSIONS(
		      void *, allocator_hazard_pointers, true, false, true, false)}
		    .length();

		m.bounds()            = sizeof(*m);
		m->heapStart          = tbase;
		m->heapStart.bounds() = tsize;
		m->heapStart.address() += msize + hazardQuarantineSize;
		m->init_bins();

		// Carve off the front of the heap space to use for the hazard
		// quarantine.
		Capability hazardQuarantine = tbase;
		hazardQuarantine.address() += msize;
		hazardQuarantine.bounds() = hazardQuarantineSize;
		m->hazardQuarantine       = hazardQuarantine.cast<void *>();

		m->mspace_firstchunk_add(
		  ds::pointer::offset<void>(tbase.get(), msize + hazardQuarantineSize),
		  tsize - msize - hazardQuarantineSize);

		return m;
	}

	void check_gm()
	{
		if (gm == nullptr)
		{
			Capability heap = const_cast<void *>(
			  MMIO_CAPABILITY_WITH_PERMISSIONS(void,
			                                   heap,
			                                   /*load*/ true,
			                                   /*store*/ true,
			                                   /*capabilities*/ true,
			                                   /*loadMutable*/ true));

			revoker.init();
			gm = mstate_init(heap, heap.bounds());
			Debug::Assert(gm != nullptr, "gm should not be null");
		}
	}

	/**
	 * Futex value to allow a thread to wait for another thread to free an
	 * object.
	 */
	cheriot::atomic<int32_t> freeFutex = -1;

	/**
	 * Helper that returns true if the timeout value permits sleeping.
	 *
	 * This assumes that the pointer was checked with `check_pointer` earlier
	 * but defends against the case where the timeout object was freed by
	 * another thread while the thread using it slept.
	 */
	bool may_block(Timeout *timeout)
	{
		return timeout->may_block();
	}

	/**
	 * Helper to reacquire the lock after sleeping.  This adds any time passed
	 * as `elapsed` to the timeout and then tries to reacquire the lock,
	 * blocking for no longer than the remaining time on this timeout.
	 *
	 * Returns `true` if the lock has been successfully reacquired, `false`
	 * otherwise.
	 */
	bool reacquire_lock(Timeout                   *timeout,
	                    LockGuard<decltype(lock)> &g,
	                    Ticks                      elapsed = 0)
	{
		timeout->elapse(elapsed);
		return g.try_lock(timeout);
	}

	/**
	 * Wait for the background revoker, if the revoker supports
	 * interrupt-driven notifications.
	 *
	 * Waits until either `timeout` expires or `epoch` has finished and then
	 * tries to reacquire the lock. Returns true if the epoch has passed and the
	 * lock has been reacquired, returns false and does *not* reacquire the lock
	 * in the case of any error.
	 *
	 */
	template<typename T = Revocation::Revoker>
	bool wait_for_background_revoker(Timeout                   *timeout,
	                                 uint32_t                   epoch,
	                                 LockGuard<decltype(lock)> &g,
	                                 T                         &r = revoker)
	    requires(Revocation::SupportsInterruptNotification<T> &&
	             Revocation::Revoker::IsAsynchronous)
	{
		// Release the lock before sleeping
		g.unlock();
		// Wait for the interrupt to fire, then try to reacquire the lock if
		// the epoch is passed.
		return r.wait_for_completion(timeout, epoch) && g.try_lock(timeout);
	}

	/**
	 * Wait for the background revoker, if the revoker does not support
	 * interrupt-driven notifications.  This will yield and retry if the revoker
	 * has not yet finished.
	 *
	 * Waits until either `timeout` expires or `epoch` has finished and then
	 * tries to reacquire the lock. Returns true if the epoch has passed and the
	 * lock has been reacquired, returns false and does *not* reacquire the lock
	 * in the case of any error.
	 *
	 */
	template<typename T = Revocation::Revoker>
	bool wait_for_background_revoker(Timeout                   *timeout,
	                                 uint32_t                   epoch,
	                                 LockGuard<decltype(lock)> &g,
	                                 T                         &r = revoker)
	    requires(!Revocation::SupportsInterruptNotification<T> ||
	             !Revocation::Revoker::IsAsynchronous)
	{
		/*
		 * Yield and poll until a revocation pass has finished.
		 *
		 * In the case of a synchronouse revoker, this amounts to doing
		 * synchronous unit of work (usually one, sometimes two) per tick so
		 * that we avoid monopolizing the allocator lock.
		 */
		while (!r.has_revocation_finished_for_epoch(epoch))
		{
			/*
			 * If we have finished an epoch and aren't yet done with the target
			 * epoch, kick off another round of revocation.  The synchronous
			 * revoker might take this opportunity to do another unit of work
			 * (in addition to the one it will do in the call to
			 * has_revocation_finished_for_epoch above).
			 */
			if ((r.system_epoch_get() & 1) == 0)
			{
				r.system_bg_revoker_kick();
			}

			if (g.yield(timeout) < 0)
			{
				/* Unable to sleep; bail out */
				return false;
			}
		}
		return true;
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
	 * reacquired during the permitted timeout then this returns -ETIMEDOUT.
	 *
	 * If `isSealedAllocation` is true, then the allocation is marked as sealed
	 * and excluded during `heap_free_all`.
	 */
	ErrorOr<void> malloc_internal(size_t                           bytes,
	                              LockGuard<decltype(lock)>      &&g,
	                              PrivateAllocatorCapabilityState *capability,
	                              Timeout                         *timeout,
	                              bool     isSealedAllocation = false,
	                              uint32_t flags              = AllocateWaitAny)
	{
		check_gm();

		do
		{
			auto ret = gm->mspace_dispatch(bytes,
			                               capability->quota,
			                               capability->identifier,
			                               isSealedAllocation);
			if (std::holds_alternative<Capability<void>>(ret))
			{
				return std::get<Capability<void>>(ret);
			}
			// Check for permanent allocation failures first.
			if (std::holds_alternative<MState::AllocationFailurePermanent>(ret))
			{
				return -EINVAL;
			}
			// If there is enough memory in the quarantine to fulfil this
			// allocation, try dequeing some things and retry.
			auto *needsRevocation =
			  std::get_if<MState::AllocationFailureRevocationNeeded>(&ret);
			if (needsRevocation)
			{
				if (!(flags & AllocateWaitRevocationNeeded))
				{
					// The flags specify that we should not wait when revocation
					// is needed. May succeed on retry
					return -EAGAIN;
				}

				if (!may_block(timeout))
				{
					return -ETIMEDOUT;
				}

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

					if (!wait_for_background_revoker(
					      timeout, needsRevocation->waitingEpoch, g))
					{
						// Failed to reacquire the lock within the allowed
						// timeout period
						return reinterpret_cast<void *>(-ETIMEDOUT);
					}
				}
				continue;
			}
			// If the heap is full, wait for someone to free an allocation and
			// then retry.
			bool isHeapFullFailure =
			  std::holds_alternative<MState::MState::AllocationFailureHeapFull>(
			    ret);
			bool isQuotaExceededFailure =
			  std::holds_alternative<MState::AllocationFailureQuotaExceeded>(
			    ret);
			if (isHeapFullFailure || isQuotaExceededFailure)
			{
				if ((isHeapFullFailure && !(flags & AllocateWaitHeapFull)) ||
				    (isQuotaExceededFailure &&
				     !(flags & AllocateWaitQuotaExceeded)))
				{
					// The flags specify that we should not
					// wait when the heap is full and/or
					// when the quota is exceeded.
					return -ENOMEM;
				}

				Debug::log("Not enough free space to handle {}-byte "
				           "allocation, sleeping",
				           bytes);
				// Use the current free space as the sleep futex value.  This
				// means that the `wait` call will fail if the amount of free
				// memory changes between dropping the lock and waiting, unless
				// a matched number of allocations and frees happen (in which
				// case, we're happy to sleep because we still can't manage
				// this allocation).
				auto expected = gm->heapFreeSize;
				freeFutex     = expected;
				// If there are things on the hazard list, wake after one tick
				// and see if they have gone away.  Otherwise, wait until we
				// have some newly freed objects.
				Timeout t{gm->hazard_quarantine_is_empty() ? timeout->remaining
				                                           : 1};
				// Drop the lock while yielding
				g.unlock();
				freeFutex.wait(&t, expected);
				timeout->elapse(t.elapsed);
				Debug::log("Woke from futex wake");
				if (!reacquire_lock(timeout, g))
				{
					// Failed to acquire lock within allowed timeout.
					return -ETIMEDOUT;
				}
				continue;
			}
		} while (may_block(timeout));
		// Exhausted the timeout period while retrying the allocation.
		return -ETIMEDOUT;
	}

	/**
	 * Unseal an allocator capability and return it.  Returns `nullptr` if this
	 * is not a heap capability.
	 */
	PrivateAllocatorCapabilityState *
	malloc_capability_unseal(AllocatorCapability in)
	{
		auto  key        = STATIC_SEALING_TYPE(MallocKey);
		auto *capability = token_unseal<AllocatorCapabilityState>(key, in);
		if (!capability)
		{
			Debug::log<DebugLevel::Warning>("Invalid malloc capability {}", in);
			return nullptr;
		}

		auto *state =
		  reinterpret_cast<PrivateAllocatorCapabilityState *>(capability);

		// Assign an identifier if this is the first time that we've seen this.
		if (state->identifier == 0)
		{
			static uint32_t nextIdentifier = 1;
			if (nextIdentifier >= (1 << MChunkHeader::OwnerIDWidth))
			{
				return nullptr;
			}
			state->identifier = nextIdentifier++;
		}
		return state;
	}

	/**
	 * Object representing a claim.  When a heap object is claimed, an instance
	 * of this structure exists to track the reference count per claimer.
	 */
	class Claim
	{
		/**
		 * The identifier of the owning allocation capability.
		 */
		uint16_t allocatorIdentifier = 0;
		/**
		 * Next 'pointer' encoded as a shifted offset from the start of the
		 * heap.
		 */
		uint16_t encodedNext = 0;
		/**
		 * Saturating reference count.  We use one to indicate a single
		 * reference count rather than zero to slightly simplify the logic at
		 * the expense of saturating one increment earlier than we need to.  It
		 * is highly unlikely that any vaguely sensible code will ever
		 * encounter the saturation case, so this is unlikely to be a problem.
		 * It might become one if we ever move to using a 16-bit reference
		 * count (which would require a different allocation path for claim
		 * structures to make sense).
		 */
		uint32_t referenceCount = 1;

		/**
		 * Private constructor, creates a new claim with a single reference
		 * count.
		 */
		Claim(uint16_t identifier, uint16_t nextClaim)
		  : allocatorIdentifier(identifier), encodedNext(nextClaim)
		{
		}

		/**
		 * Destructor is private, claims should always be destroyed via
		 * `destroy`.
		 */
		~Claim() = default;

		friend class Iterator;

		public:
		/**
		 * Returns the owner of this claim.
		 */
		[[nodiscard]] uint16_t owner() const
		{
			return allocatorIdentifier;
		}

		/**
		 * Returns the value of the compressed next pointer.
		 */
		[[nodiscard]] uint16_t encoded_next() const
		{
			return encodedNext;
		}

		/**
		 * Claims list iterator.  This wraps a next pointer and so can be used
		 * both to inspect a value and update it.
		 */
		class Iterator
		{
			/**
			 * Placeholder value for end iterators.
			 */
			static inline const uint16_t EndPlaceholder = 0;

			/**
			 * A pointer to the encoded next pointer.
			 */
			uint16_t *encodedNextPointer =
			  const_cast<uint16_t *>(&EndPlaceholder);

			public:
			/**
			 * Default constructor returns a generic end iterator.
			 */
			Iterator() = default;

			/// Copy constructor.
			__always_inline Iterator(const Iterator &other) = default;

			/// Constructor from an explicit next pointer.
			__always_inline Iterator(uint16_t *nextPointer)
			  : encodedNextPointer(nextPointer)
			{
			}

			/**
			 * Dereference.  Returns the claim that this iterator points to.
			 */
			__always_inline Claim *operator*()
			{
				return Claim::from_encoded_offset(*encodedNextPointer);
			}

			/**
			 * Dereference.  Returns the claim that this iterator points to.
			 */
			__always_inline Claim *operator->()
			{
				return Claim::from_encoded_offset(*encodedNextPointer);
			}

			/// Iteration termination condition.
			__always_inline bool operator!=(const Iterator Other)
			{
				return *encodedNextPointer != *Other.encodedNextPointer;
			}

			/**
			 * Preincrement, moves to the next element.
			 */
			Iterator &operator++()
			{
				Claim *next        = **this;
				encodedNextPointer = &next->encodedNext;
				return *this;
			}

			/**
			 * Assignment, replaces the claim that this iterator points to with
			 * the new one.
			 */
			Iterator &operator=(Claim *claim)
			{
				*encodedNextPointer = claim->encode_address();
				return *this;
			}

			/**
			 * Returns the next pointer that this iterator refers to.
			 */
			uint16_t *pointer()
			{
				return encodedNextPointer;
			}
		};

		/**
		 * Allocate a new claim.  This will fail if space is not immediately
		 * available.
		 *
		 * Returns a pointer to the new allocation on success, nullptr on
		 * failure.
		 */
		static Claim *create(PrivateAllocatorCapabilityState &capability,
		                     uint16_t                         next)
		{
			auto space = gm->mspace_dispatch(
			  sizeof(Claim), capability.quota, capability.identifier);
			if (!std::holds_alternative<Capability<void>>(space))
			{
				return nullptr;
			}
			return new (std::get<Capability<void>>(space))
			  Claim(capability.identifier, next);
		}

		/**
		 * Destroy a claim, which must have been allocated with `capability`.
		 */
		static void destroy(PrivateAllocatorCapabilityState &capability,
		                    Claim                           *claim)
		{
			Capability heap{gm->heapStart};
			heap.address() = Capability{claim}.address();
			auto chunk     = MChunkHeader::from_body(heap);
			capability.quota += chunk->size_get();
			// We could skip quarantine for these objects, since we know that
			// they haven't escaped, but they're small so it's probably not
			// worthwhile.
			gm->mspace_free(*chunk, sizeof(Claim));
		}

		/**
		 * Add a reference.  If this would overflow, the reference is pinned
		 * and this never decrements.
		 */
		void reference_add()
		{
			if (referenceCount !=
			    std::numeric_limits<decltype(referenceCount)>::max())
			{
				referenceCount++;
			}
		}

		/**
		 * Decrement the reference count and return whether this has dropped
		 * the reference count to 0.
		 */
		bool reference_remove()
		{
			if (referenceCount !=
			    std::numeric_limits<decltype(referenceCount)>::max())
			{
				referenceCount--;
			}
			return referenceCount == 0;
		}

		/**
		 * Decode an encoded offset and return a pointer to the claim.
		 */
		static Claim *from_encoded_offset(uint16_t offset)
		{
			if (offset == 0)
			{
				return nullptr;
			}
			Capability<Claim> ret{gm->heapStart.cast<Claim>()};
			ret.address() += offset << MallocAlignShift;
			ret.bounds() = sizeof(Claim);
			return ret;
		}

		/**
		 * Encode the address of this object in a 16-bit value.
		 */
		uint16_t encode_address()
		{
			ptraddr_t address = Capability{this}.address();
			address -= gm->heapStart.address();
			Debug::Assert((address & MallocAlignMask) == 0,
			              "Claim at address {} is insufficiently aligned",
			              address);
			address >>= MallocAlignShift;
			Debug::Assert(address <= std::numeric_limits<uint16_t>::max(),
			              "Encoded claim address is too large: {}",
			              address);
			return address;
		}
	};
	static_assert(sizeof(Claim) <= (1 << MallocAlignShift),
	              "Claims should fit in the smallest possible allocation");

	/**
	 * Find a claim if one exists.  Returns a reference to the next pointer
	 * that refers to this claim.
	 */
	std::pair<uint16_t &, Claim *> claim_find(uint16_t      owner,
	                                          MChunkHeader &chunk)
	{
		for (Claim::Iterator i{&chunk.claims}, end; i != end; ++i)
		{
			Claim *claim = *i;
			if (claim->owner() == owner)
			{
				return {*i.pointer(), claim};
			}
		}
		return {chunk.claims, nullptr};
	}

	/**
	 * Add a claim to a chunk, owned by `owner`.  This returns true if the
	 * claim was successfully added, false otherwise.
	 */
	bool claim_add(PrivateAllocatorCapabilityState &owner, MChunkHeader &chunk)
	{
		Debug::log("Adding claim for {}", owner.identifier);
		auto [next, claim] = claim_find(owner.identifier, chunk);
		if (claim)
		{
			Debug::log("Adding second claim");
			claim->reference_add();
			return true;
		}
		bool   isOwner = (chunk.ownerID == owner.identifier);
		size_t size    = chunk.size_get();
		if (!isOwner)
		{
			if (owner.quota < size)
			{
				Debug::log<DebugLevel::Warning>("quota insufficient");
				return false;
			}
			owner.quota -= size;
		}
		claim = Claim::create(owner, next);
		if (claim != nullptr)
		{
			Debug::log("Allocated new claim");
			// If this is the owner, remove the owner and downgrade our
			// ownership to a claim.  This simplifies the deallocation path.
			if (isOwner)
			{
				chunk.ownerID = 0;
				claim->reference_add();
			}
			next = claim->encode_address();
			return true;
		}
		// If we failed to allocate the claim object, undo adding this to our
		// quota.
		if (!isOwner)
		{
			owner.quota += size;
		}
		Debug::log<DebugLevel::Warning>("Failed to add claim");
		return false;
	}

	/**
	 * Drop a claim on an object by the specified allocator capability.  If
	 * `reallyDrop` is false then this does not actually drop the claim but
	 * returns true if it *could have* dropped a claim.
	 * Returns true if a claim was dropped, false otherwise.
	 */
	bool claim_drop(PrivateAllocatorCapabilityState &owner,
	                MChunkHeader                    &chunk,
	                bool                             reallyDrop)
	{
		Debug::log(
		  "Trying to drop claim with {} ({})", owner.identifier, &owner);
		auto [next, claim] = claim_find(owner.identifier, chunk);
		// If there is no claim, fail.
		if (claim == nullptr)
		{
			return false;
		}
		if (!reallyDrop)
		{
			return true;
		}
		// Drop the reference.  If this results in the last reference going
		// away, destroy this claim structure.
		if (claim->reference_remove())
		{
			next        = claim->encoded_next();
			size_t size = chunk.size_get();
			owner.quota += size;
			Claim::destroy(owner, claim);
			Debug::log("Dropped last claim, refunding {}-byte quota for {}",
			           size,
			           chunk.body());
		}
		return true;
	}

	/**
	 * Having found a chunk, try to free it with the provided owner.  The size
	 * of the chunk is provided by the caller as `bodySize`.  If `isPrecise` is
	 * false then this will drop a claim but will not free the object as the
	 * owner.  If `reallyFree` is false then this will not actually perform the
	 * operation it will simply report whether it *would* succeed.
	 *
	 * Returns 0 on success, `-EPERM` if the provided owner cannot free this
	 * chunk.
	 */
	__noinline int heap_free_chunk(PrivateAllocatorCapabilityState &owner,
	                               MChunkHeader                    &chunk,
	                               size_t                           bodySize,
	                               bool isPrecise  = true,
	                               bool reallyFree = true)
	{
		// If this is a precise allocation, see if we can free it as the
		// original owner.  You may drop claims with a capability that is a
		// subset of the original but you may not free an object with a subset.
		if (isPrecise && (chunk.owner() == owner.identifier))
		{
			if (!reallyFree)
			{
				return 0;
			}
			size_t chunkSize = chunk.size_get();
			chunk.ownerID    = 0;
			if (chunk.claims == 0)
			{
				int ret = gm->mspace_free(chunk, bodySize);
				// If free fails, don't manipulate the quota.
				if (ret == 0)
				{
					owner.quota += chunkSize;
				}
				return ret;
			}
			// We've removed the owner, so refund the quota immediately, the
			// free won't happen until the last claim goes away, but this is no
			// longer the owner's responsibility.
			owner.quota += chunkSize;
			return 0;
		}
		// If this is an interior (but valid) pointer, see if we can drop a
		// claim.
		if (claim_drop(owner, chunk, reallyFree))
		{
			if ((chunk.claims == 0) && (chunk.ownerID == 0))
			{
				return gm->mspace_free(chunk, bodySize);
			}
			return 0;
		}
		return -EPERM;
	}

	__noinline int heap_free_internal(AllocatorCapability heapCapability,
	                                  void               *rawPointer,
	                                  bool                reallyFree)
	{
		auto *capability = malloc_capability_unseal(heapCapability);
		if (capability == nullptr)
		{
			Debug::log<DebugLevel::Warning>("Invalid heap capability {}",
			                                heapCapability);
			return -EPERM;
		}
		// Validate the pointer. We do not permit freeing sealed pointers.
		Capability<void> mem{rawPointer};
		if (!mem.is_valid() || mem.is_sealed())
		{
			return -EINVAL;
		}
		check_gm();
		/*
		 * Find the chunk that corresponds to this allocation.  We use the base
		 * as it cannot be further than one-past-the-end (because CHERI bounds
		 * are monotone non-increasing). For us, one-past-the-end is the next
		 * chunk's header which will have the revocation bit set meaning
		 * allocation_start would return null. Note: `rawPointer`'s address is
		 * not used at all!
		 */
		auto *chunk = gm->allocation_start(mem.base());
		if (!chunk)
		{
			return -EINVAL;
		}
		ptraddr_t start    = chunk->body().address();
		size_t    bodySize = gm->chunk_body_size(*chunk);
		// Is the pointer that we're freeing a pointer to the entire allocation?
		bool isPrecise = (start == mem.base()) && (bodySize == mem.length());
		return heap_free_chunk(
		  *capability, *chunk, bodySize, isPrecise, reallyFree);
	}

} // namespace

__cheriot_minimum_stack(0xa0) ssize_t
  heap_quota_remaining(AllocatorCapability heapCapability)
{
	STACK_CHECK(0xa0);
	LockGuard g{lock};
	auto     *cap = malloc_capability_unseal(heapCapability);
	if (cap == nullptr)
	{
		return -EPERM;
	}
	return cap->quota;
}

__cheriot_minimum_stack(0xe0) int heap_quarantine_flush(Timeout *timeout)
{
	STACK_CHECK(0xe0);

	if (!check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}

	if (LockGuard g{lock, timeout})
	{
		auto epoch = revoker.system_epoch_get();
		// Round the epoch up.  Odd epoch numbers indicate in-progress epochs.
		epoch = (epoch + 1) & ~1U;
		// If we're using an asynchronous revoker, kick it now.  We'll
		// probably want to kick it later anyway, so we don't lose anything
		// by starting it early.
		if constexpr (Revocation::Revoker::IsAsynchronous)
		{
			revoker.system_bg_revoker_kick();
		}
		// Try removing items from quarantine until we've popped all that
		// we can.  There may still be quarantine things from the previous
		// epoch.
		while (gm->quarantine_dequeue()) {}
		// If we've emptied the quarantine, stop and report success.
		if (gm->heapQuarantineSize == 0)
		{
			return 0;
		}
		// Wait for the revocation epoch to expire.
		if (!wait_for_background_revoker(timeout, epoch, g))
		{
			return -ETIMEDOUT;
		}
		// Remove everything that was freed with the previous revocation.
		while (gm->quarantine_dequeue()) {}
		Debug::log("{} bytes left in quarantine", gm->heapQuarantineSize);
		return 0;
	}

	return -ETIMEDOUT;
}

__cheriot_minimum_stack(0x220) void *heap_allocate(
  Timeout            *timeout,
  AllocatorCapability heapCapability,
  size_t              bytes,
  uint32_t            flags)
{
	STACK_CHECK(0x220);
	if (!check_timeout_pointer(timeout))
	{
		return reinterpret_cast<void *>(-EINVAL);
	}
	LockGuard g{lock};
	auto     *cap = malloc_capability_unseal(heapCapability);
	if (cap == nullptr)
	{
		return reinterpret_cast<void *>(-EPERM);
	}
	// Use the default memory space.
	return malloc_internal(bytes, std::move(g), cap, timeout, false, flags)
	  .as_raw();
}

__cheriot_minimum_stack(0x1c0) ssize_t
  heap_claim(AllocatorCapability heapCapability, void *pointer)
{
	STACK_CHECK(0x1c0);
	LockGuard g{lock};
	auto     *cap = malloc_capability_unseal(heapCapability);
	if (cap == nullptr)
	{
		Debug::log<DebugLevel::Warning>("Invalid heap cap");
		return -EPERM;
	}
	Capability p{pointer};
	if (!p.is_valid() || p.is_sealed())
	{
		Debug::log<DebugLevel::Warning>("Invalid claimed cap");
		return -EINVAL;
	}
	/*
	 * As with heap_free we use the base to lookup the chunk (see comments in
	 * heap_free_internal).
	 */
	auto *chunk = gm->allocation_start(p.base());
	if (chunk == nullptr)
	{
		Debug::log<DebugLevel::Warning>("chunk not found");
		return -EINVAL;
	}
	if (claim_add(*cap, *chunk))
	{
		return gm->chunk_body_size(*chunk);
	}
	Debug::log<DebugLevel::Warning>("failed to add claim");
	return -ENOMEM;
}

static constexpr size_t HeapFreeStackUsage = 0x260;

__cheriot_minimum_stack(HeapFreeStackUsage) int heap_can_free(
  AllocatorCapability heapCapability,
  void               *rawPointer)
{
	// This function requires much less space, but we claim that we require as
	// much as `heap_free` so that a call to `heap_free` will not fail due to
	// insufficient stack immediately after `heap_can_free` has said that it's
	// fine.
	STACK_CHECK(HeapFreeStackUsage);
	LockGuard g{lock};
	return heap_free_internal(heapCapability, rawPointer, false);
}

int heap_free_nostackcheck(AllocatorCapability heapCapability, void *rawPointer)
{
	LockGuard g{lock};
	int       ret = heap_free_internal(heapCapability, rawPointer, true);
	if (ret != 0)
	{
		return ret;
	}

	// If there are any threads blocked allocating memory, wake them up.
	if (freeFutex != -1)
	{
		Debug::log("Some threads are blocking on allocations, waking them");
		freeFutex = -1;
		freeFutex.notify_all();
	}

	return 0;
}

__cheriot_minimum_stack(HeapFreeStackUsage) int heap_free(
  AllocatorCapability heapCapability,
  void               *rawPointer)
{
	STACK_CHECK(HeapFreeStackUsage);
	return heap_free_nostackcheck(heapCapability, rawPointer);
}

__cheriot_minimum_stack(0x1a0) ssize_t
  heap_free_all(AllocatorCapability heapCapability)
{
	STACK_CHECK(0x1a0);
	LockGuard g{lock};
	auto     *capability = malloc_capability_unseal(heapCapability);
	if (capability == nullptr)
	{
		Debug::log<DebugLevel::Warning>("Invalid heap capability {}",
		                                heapCapability);
		return -EPERM;
	}

	auto      chunk   = gm->heapStart.cast<MChunkHeader>();
	ptraddr_t heapEnd = chunk.top();
	ssize_t   freed   = 0;
	do
	{
		if (chunk->is_in_use() && !chunk->isSealedObject)
		{
			auto size = chunk->size_get();
			if (heap_free_chunk(
			      *capability, *chunk, gm->chunk_body_size(*chunk)) == 0)
			{
				freed += size;
			}
		}
		chunk = static_cast<MChunkHeader *>(chunk->cell_next());
	} while (chunk.address() < heapEnd);

	// If there are any threads blocked allocating memory, wake them up.
	if ((freeFutex > 0) && (freed > 0))
	{
		Debug::log("Some threads are blocking on allocations, waking them");
		freeFutex = 0;
		freeFutex.notify_all();
	}

	return freed;
}

__cheriot_minimum_stack(0x230) void *heap_allocate_array(
  Timeout            *timeout,
  AllocatorCapability heapCapability,
  size_t              nElements,
  size_t              elemSize,
  uint32_t            flags)
{
	STACK_CHECK(0x230);
	if (!check_timeout_pointer(timeout))
	{
		return reinterpret_cast<void *>(-EINVAL);
	}
	LockGuard g{lock};
	auto     *cap = malloc_capability_unseal(heapCapability);
	if (cap == nullptr)
	{
		return reinterpret_cast<void *>(-EPERM);
	}
	// Use the default memory space.
	size_t req;
	if (__builtin_mul_overflow(nElements, elemSize, &req))
	{
		return reinterpret_cast<void *>(-EINVAL);
	}
	return malloc_internal(req, std::move(g), cap, timeout, false, flags)
	  .as_raw();
}

namespace
{
	/**
	 * The next sealing type to allocate.  Sealing types are allocated from the
	 * top down.
	 */
	uint32_t nextSealingType = std::numeric_limits<uint32_t>::max();

	/// A type alias for a sealed capability to a token object.
	using SealedTokenHandle = CHERI::Capability<TokenObjectType, true>;

	/*
	 * A type alias for an unsealed capability to a token object.  The template
	 * parameter distinguishes whether the token object header is in bounds
	 * (that is, whether this capability must be considered secret).
	 */
	template<bool HeaderInBounds>
	using TokenHandle = CHERI::Capability<TokenObjectType, false>;

	/**
	 * Helper that allocates a sealed object and returns the sealed and
	 * unsealed capabilities to the object.  Requires that the sealing key have
	 * all of the permissions in `permissions`.
	 */
	std::pair<SealedTokenHandle, TokenHandle<false>>
	  __noinline allocate_sealed_unsealed(Timeout            *timeout,
	                                      AllocatorCapability heapCapability,
	                                      SealingKey          key,
	                                      size_t              requestedSize,
	                                      PermissionSet       permissions)
	{
		if (!check_timeout_pointer(timeout))
		{
			return {nullptr, nullptr};
		}

		if (!permissions.can_derive_from(key.permissions()))
		{
			Debug::log<DebugLevel::Warning>(
			  "Operation requires {}, cannot derive from {}", permissions, key);
			return {nullptr, nullptr};
		}

		// Round up the size to the next representable size.  This ensures
		// that, once we've added the header space, we have an allocation where
		// both the object (from the end) and the header (with padding at the
		// start) are representable.
		size_t unsealedSize = CHERI::representable_length(requestedSize);
		// Very large sizes may be rounded 'up' to zero.  Don't allow this.
		if (unsealedSize == 0)
		{
			Debug::log<DebugLevel::Warning>(
			  "Requested size {} is not representable", requestedSize);
			return {nullptr, nullptr};
		}

		// It shouldn't be possible to overflow the add due to the way the
		// rounding works, but this is not guaranteed in future capability
		// encodings, so we'll do a tiny bit of extra work here to avoid
		// accidentally introducing a security vulnerability in a future
		// encoding.
		size_t sealedSize;
		if (__builtin_add_overflow(
		      sizeof(TokenObjectHeader), unsealedSize, &sealedSize))
		{
			Debug::log<DebugLevel::Warning>(
			  "Requested size {} is too large to include header",
			  requestedSize);
			return {nullptr, nullptr};
		}

		LockGuard g{lock};
		auto     *capability = malloc_capability_unseal(heapCapability);
		if (capability == nullptr)
		{
			return {nullptr, nullptr};
		}

		return malloc_internal(
		         sealedSize, std::move(g), capability, timeout, true)
		  .either(
		    [key, sealedSize](auto alloc) {
			    // Allocate space at the top for the payload.
			    auto headerThenPayload =
			      alloc.template cast<TokenObjectHeader>();
			    headerThenPayload.address() =
			      headerThenPayload.top() - sealedSize;

			    // Round down the base to the heap alignment size, padding the
			    // payload. This ensures that the header is aligned and gives
			    // the same alignment as a normal allocation.  We will already
			    // be this aligned for most requested sizes.  The allocator
			    // always aligns the top and bottom of any allocations on a
			    // `MallocAlignment` boundary, and also on a representable
			    // boundary (whichever is stricter).  This extra alignment step
			    // ensures that this is also true for both the start of the
			    // header and the start of the unsealed object.  If the
			    // representable-length rounding of the requested size increased
			    // the requested alignment beyond 8, this will be a no-op.
			    headerThenPayload.align_down(MallocAlignment);

			    headerThenPayload->type = key.address();

			    TokenHandle<true> payloadAfterHeader{
			      reinterpret_cast<TokenObjectType *>(headerThenPayload.get())};
			    payloadAfterHeader.address() += sizeof(TokenObjectHeader);

			    // Sealed points at payload but can access header, at negative
			    // offset.
			    auto sealed = [=]() {
				    auto res{payloadAfterHeader};
				    // Set all of the user permissions
				    res.address() += 7;
				    return res.seal(allocatorSealingKey);
			    }();

			    // Unsealed points at payload and can access only payload
			    TokenHandle<false> payload{payloadAfterHeader.get()};
			    payload.bounds() = payload.top() - payload.address();

			    Debug::Assert(payload.is_valid(),
			                  "Unsealed object {} is not representable",
			                  payload);
			    return std::pair<SealedTokenHandle, TokenHandle<false>>{
			      sealed, payload};
		    },
		    [](int e) {
			    Debug::log<DebugLevel::Warning>(
			      "Underlying allocation failed for sealed object {}", e);
			    return std::pair<SealedTokenHandle, TokenHandle<false>>{
			      nullptr, nullptr};
		    });
	}
} // namespace

TokenKey token_key_new()
{
	// This needs protecting against races but doesn't touch any other data
	// structures and so can have its own lock.
	static FlagLock tokenLock;
	LockGuard       g{tokenLock};
	auto            keyRoot = softwareSealingKey;
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

__cheriot_minimum_stack(0x290) CHERI_SEALED(void *)
  token_sealed_unsealed_alloc(Timeout            *timeout,
                              AllocatorCapability heapCapability,
                              TokenKey            key,
                              size_t              sz,
                              void              **unsealed)
{
	STACK_CHECK(0x290);
	if (!check_timeout_pointer(timeout))
	{
		return nullptr;
	}
	auto [sealed, obj] = allocate_sealed_unsealed(
	  timeout, heapCapability, key, sz, {Permission::Seal, Permission::Unseal});
	{
		/*
		 * Write the unsealed capability through the out parameter, while
		 * holding the allocator lock.  That's a little heavy-handed, but it
		 * suffices to ensure that it won't be freed out from under us, so
		 * if it passes `check_pointer`, then the store won't trap.
		 */
		LockGuard g{lock};
		if (check_pointer<PermissionSet{
		      Permission::Store, Permission::LoadStoreCapability}>(unsealed))
		{
			*unsealed = obj;
		}
	}
	/*
	 * Regardless of whether we were able to store the unsealed pointer, return
	 * the sealed object.
	 */
	return sealed;
}

__cheriot_minimum_stack(0x260) CHERI_SEALED(void *)
  token_sealed_alloc(Timeout            *timeout,
                     AllocatorCapability heapCapability,
                     TokenKey            rawKey,
                     size_t              sz)
{
	STACK_CHECK(0x260);
	return allocate_sealed_unsealed(
	         timeout, heapCapability, rawKey, sz, {Permission::Seal})
	  .first;
}

/**
 * Helper used to unseal a sealed object with a given key.  Performs all of the
 * relevant checks and returns nullptr if this is not a valid key and object
 * sealed with that key.
 *
 * Call with the allocator lock held.
 */
__noinline static TokenHandle<true>
unseal_internal(TokenKey key, CHERI_SEALED(TokenObjectType *) object)
{
	Capability unsealedInner{token_obj_unseal_dynamic(key, object)};
	if (!unsealedInner.is_valid())
	{
		return nullptr;
	}

	/*
	 * The call to token_obj_unseal_dynamic above has already done this unseal,
	 * but will not share the result with us.  That said, if we get here, the
	 * object capability was tagged and unsealed correctly inside that call.
	 * Since we (by assumption) hold the allocator lock, there is no TOCTTOU and
	 * it remains tagged and unsealable; it is, in particular, not possible that
	 * the underlying allocation has been freed between that check and now, so
	 * we need not check the result of this unseal for validity.
	 *
	 * It has also already checked that the key is tagged, has positive length,
	 * has address equal to its base, and grants unseal (US) permission.  These
	 * are properties of the capability, which is not subject to revocation, and
	 * so still hold.
	 */
	return Capability{object}.unseal(allocatorSealingKey);
}

static constexpr size_t TokenObjDestroyStackUsage = 0x270;

__cheriot_minimum_stack(TokenObjDestroyStackUsage) int token_obj_destroy(
  AllocatorCapability heapCapability,
  TokenKey            key,
  CHERI_SEALED(void *) object)
{
	STACK_CHECK(TokenObjDestroyStackUsage);
	TokenHandle<true> unsealed;
	{
		LockGuard g{lock};
		unsealed = unseal_internal(
		  key, reinterpret_cast<CHERI_SEALED(TokenObjectType *)>(object));
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

	// The sealed pointer was an interior address; move back to the base
	unsealed.address() = unsealed.base();

	return heap_free_nostackcheck(heapCapability, unsealed);
}

__cheriot_minimum_stack(TokenObjDestroyStackUsage) int token_obj_can_destroy(
  AllocatorCapability heapCapability,
  TokenKey            key,
  CHERI_SEALED(void *) object)
{
	STACK_CHECK(TokenObjDestroyStackUsage);
	TokenHandle<true> unsealed;
	{
		LockGuard g{lock};
		unsealed = unseal_internal(
		  key, reinterpret_cast<CHERI_SEALED(TokenObjectType *)>(object));
		if (unsealed == nullptr)
		{
			return -EINVAL;
		}
		// At this point, we drop and reacquire the lock. This is better for
		// code reuse and heap_can_free will catch races because it will check
		// the revocation state.
		// The key can't be revoked and so there is no race with the key going
		// away after the check.
	}

	// The sealed pointer was an interior address; move back to the base
	unsealed.address() = unsealed.base();

	return heap_can_free(heapCapability, unsealed);
}

size_t heap_available()
{
	return gm->heapFreeSize;
}

[[cheriot::interrupt_state(disabled)]] int heap_render()
{
#if HEAP_RENDER
	LockGuard g{lock};
	check_gm();
	gm->render();
#endif
	return 0;
}

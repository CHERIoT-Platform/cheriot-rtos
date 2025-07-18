// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
// Use a large quota for this compartment.
#define MALLOC_QUOTA 0x100000
#define TEST_NAME "Allocator"

#include "tests.hh"
#include <cheri.hh>
#include <cheriot-atomic.hh>
#include <cstdlib>
#include <debug.hh>
#include <ds/xoroshiro.h>
#include <errno.h>
#include <futex.h>
#include <global_constructors.hh>
#include <switcher.h>
#include <thread.h>
#include <thread_pool.h>
#include <token.h>
#include <vector>

using thread_pool::async;
#define SECOND_HEAP_QUOTA 1024U
DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(secondHeap, SECOND_HEAP_QUOTA);
using namespace CHERI;
#define SECOND_HEAP STATIC_SEALED_VALUE(secondHeap)

DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(emptyHeap, 0);
#define EMPTY_HEAP STATIC_SEALED_VALUE(emptyHeap)

/* Used to test that the revoker sweeps static sealed capabilities */
struct AllocatorTestStaticSealedType
{
	void *pointer;
};
DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(struct AllocatorTestStaticSealedType,
                                       allocator_test,
                                       AllocatorTestCapabilityType,
                                       allocatorTestStaticSealedValue,
                                       0);

namespace
{
	/**
	 * Maximum timeout for a blocking malloc.  This needs to be large enough
	 * that we can do a complete revocation sweep in this many ticks but small
	 * enough that we don't cause CI to block forever.
	 */
	constexpr size_t AllocTimeout = 1 << 8;

	Timeout noWait{0};

	constexpr size_t MaxAllocCount = 16;
	constexpr size_t TestIterations =
#ifdef NDEBUG
	  32
#else
	  8
#endif
	  ;

	std::vector<void *> allocations;

	/**
	 * Quick check of basic functionality before we get too carried away
	 *
	 * This is marked as noinline because otherwise the predict-false on the
	 * test failures causes all of the log-message-and-fail blocks to be moved
	 * to the end of the function and, if this is inlined, ends up with some
	 * branches that are more than 2 KiB away from their targets.
	 */
	__noinline void test_preflight()
	{
		Timeout t{5};
		void *volatile p = heap_allocate(&t, MALLOC_CAPABILITY, 16);
		TEST(Capability{p}.is_valid(), "Unable to make first allocation");

		int res = heap_free(MALLOC_CAPABILITY, p);
		TEST_EQUAL(res, 0, "heap_free returned nonzero");
		TEST(
		  !Capability{p}.is_valid_temporal(),
		  "Freed pointer still live; load barrier or revoker out of service?");
	}

	/**
	 * Test the revoker by constantly allocating and freeing batches of
	 * allocations. The total amount of allocations must greatly exceed the heap
	 * size to force a constant stream of allocation failures and revocations.
	 * The time required to finish the test indicates revoker performance, lower
	 * the better.
	 *
	 * This performance test should not fail. If it fails it's either the
	 * allocations in one iteration exceed the total heap size, or the revoker
	 * is buggy or too slow.
	 *
	 * This is marked as noinline because otherwise the predict-false on the
	 * test failures causes all of the log-message-and-fail blocks to be moved
	 * to the end of the function and, if this is inlined, ends up with some
	 * branches that are more than 2 KiB away from their targets.
	 */
	__noinline void test_revoke(const size_t HeapSize)
	{
#ifdef TEMPORAL_SAFETY
		{
			static cheriot::atomic<int> state = 2;
			int                         sleeps;

			void *volatile pStack = malloc(16);
			TEST(__builtin_cheri_tag_get(pStack),
			     "Failed to allocate for test");

			static void *volatile pGlobal = malloc(16);
			TEST(__builtin_cheri_tag_get(pGlobal),
			     "Failed to allocate for test");

			auto unsealedToken = token_unseal<AllocatorTestStaticSealedType>(
			  STATIC_SEALING_TYPE(AllocatorTestCapabilityType),
			  STATIC_SEALED_VALUE(allocatorTestStaticSealedValue));
			unsealedToken->pointer = pGlobal;

			/*
			 * Check that trusted stacks are swept.  This one is fun: we need to
			 * somehow ensure that a freed pointer is in the register file of an
			 * off-core thread.  async() and inline asm it is!
			 */
			async([=]() {
				int ptag, scratch;

				/*
				 * Release the main thread, then busy spin, ensuring that our
				 * test pointer is in a register throughout, then get its tag.
				 */
				__asm__ volatile("csw zero, 0(%[state])\n"
				                 "1:\n"
				                 "clw %[scratch], 0(%[state])\n"
				                 "beqz %[scratch], 1b\n"
				                 "cgettag %[out], %[p]\n"
				                 : [out] "+&r"(ptag), [scratch] "=&r"(scratch)
				                 : [p] "C"(pGlobal), [state] "C"(&state));

				TEST(ptag == 0, "Revoker failed to sweep trusted stack");

				/* Release the main thread */
				state = 3;
			});

			sleeps = 0;
			while (state.load() != 0)
			{
				TEST(sleep(1) >= 0, "Failed to sleep");
				TEST(sleeps++ < 100, "Background thread not ready");
			}

			free(pStack);
			free(pGlobal);
			TEST_SUCCESS(heap_quarantine_empty());

			state = 1;

#	if __has_builtin(__builtin_cheri_tag_get_temporal)
			/* Check that globals are swept */
			TEST(!__builtin_cheri_tag_get_temporal(pGlobal),
			     "Revoker failed to sweep globals");

			/* Check that the stack is swept */
			TEST(!__builtin_cheri_tag_get_temporal(pStack),
			     "Revoker failed to sweep stack");

			TEST(!__builtin_cheri_tag_get_temporal(unsealedToken->pointer),
			     "Revoker failed to sweep static sealed cap");
#	else
			debug_log("Skipped temporal safety advanced handling checks");
#	endif

			/* Wait for the async thread to have performed its test */
			sleeps = 0;
			while (state.load() != 3)
			{
				TEST(sleep(1) >= 0, "Failed to sleep");
				TEST(sleeps++ < 100, "Background thread not finished");
			}
		}
#else
		debug_log("Skipping temporal safety checks");
#endif

		const size_t AllocSize = HeapSize / MaxAllocCount;

		/* Repeatedly cycle quarantine */
		allocations.resize(MaxAllocCount);

		/* Do one round to count how many allocations actually fit */
		{
			size_t i = 0;

			for (auto &allocation : allocations)
			{
				Timeout t{AllocTimeout};
				allocation = heap_allocate(&t, MALLOC_CAPABILITY, AllocSize);

				if (!__builtin_cheri_tag_get(allocation))
				{
					break;
				}

				i++;
			}

			allocations.resize(i);

			for (auto &allocation : allocations)
			{
				free(allocation);
			}

			TEST(i >= (MaxAllocCount / 2), "Heap is implausibly fragmented?");
		}

		debug_log("test_revoke using rounds of {} {}-byte objects",
		          allocations.size(),
		          AllocSize);

		for (size_t i = 0; i < TestIterations; ++i)
		{
			for (auto &allocation : allocations)
			{
				Timeout t{AllocTimeout};
				allocation = heap_allocate(&t, MALLOC_CAPABILITY, AllocSize);
				TEST(
				  __builtin_cheri_tag_get(allocation),
				  "Cannot make allocations anymore. Either the revoker is not "
				  "working or it's too slow");
			}
			for (auto allocation : allocations)
			{
				free(allocation);
			}
#ifdef TEMPORAL_SAFETY
			for (auto allocation : allocations)
			{
				TEST(
				  !__builtin_cheri_tag_get(allocation),
				  "tag for freed memory {} from allocation {} should be clear",
				  allocation);
			}
#else
			debug_log("Skipping tag checks on freed allocations because "
			          "temporal safety is not supported.");
#endif
			debug_log(
			  "Checked that all allocations have been deallocated ({} of {})",
			  static_cast<int>(i),
			  static_cast<int>(TestIterations));
			TEST(sleep(1) >= 0, "Failed to sleep");
		}
		allocations.clear();
	}

	cheriot::atomic<uint32_t> freeStart;
	/**
	 * Test that we can do a long-running blocking allocation in one thread and
	 * a free in another thread and make forward progress.
	 */
	void test_blocking_allocator(const size_t HeapSize)
	{
		/**
		 * Size of an allocation that is big enough that we'll exhaust memory
		 * before we allocate `MaxAllocCount` of them.
		 */
		const size_t BigAllocSize = HeapSize / (MaxAllocCount - 1);
		TEST(BigAllocSize > 0,
		     "Cannot guestimate big allocation size for our heap of {} bytes",
		     HeapSize);
		debug_log("BigAllocSize {} bytes", BigAllocSize);

		allocations.resize(MaxAllocCount);
		// Create the background worker before we try to exhaust memory.
		async([]() {
			// Make sure that we reach the blocking free.
			debug_log("Deallocation thread sleeping");
			freeStart.wait(0);
			// One extra sleep to make sure that we're really in the blocking
			// sleep.
			TEST(sleep(2) >= 0, "Failed to sleep");
			debug_log(
			  "Deallocation thread resuming, freeing pool of allocations");
			// Free all of the allocations to make space.
			for (auto &allocation : allocations)
			{
				if (allocation != nullptr)
				{
					TEST_EQUAL(heap_free(MALLOC_CAPABILITY, allocation),
					           0,
					           "Could not free allocation");
				}
			}
			// Notify the parent thread that we're done.
			freeStart = 2;
			freeStart.notify_one();
		});

		/*
		 * Empty the allocator's quarantine so that we're sure that the nullptr
		 * failure we see below isn't because we aren't allowing the revocation
		 * state machine to advance.
		 */
		TEST_SUCCESS(heap_quarantine_empty());

		bool memoryExhausted = false;
		for (auto &allocation : allocations)
		{
			allocation =
			  heap_allocate(&noWait, MALLOC_CAPABILITY, BigAllocSize);
			// here test for nullptr (as opposed to the valid tag
			// bit) because we specifically want to check for OOM
			if (allocation == nullptr)
			{
				memoryExhausted = true;
				break;
			}
		}
		TEST(memoryExhausted, "Failed to exhaust memory");
		debug_log("Calling heap_render");
		TEST_EQUAL(heap_render(), 0, "heap_render returned non-zero");
		debug_log("Trying a non-blocking allocation");
		// nullptr check because we explicitly want to check for OOM
		TEST(heap_allocate(&noWait, MALLOC_CAPABILITY, BigAllocSize) == nullptr,
		     "Non-blocking heap allocation did not return failure with memory "
		     "exhausted");
		debug_log("Checking that the 'heap full' flag works");
		Timeout forever{UnlimitedTimeout};
		TEST(heap_allocate(&forever,
		                   MALLOC_CAPABILITY,
		                   BigAllocSize,
		                   AllocateWaitRevocationNeeded |
		                     AllocateWaitQuotaExceeded) == nullptr,
		     "Blocking heap allocation with the heap full flag unset did not "
		     "return failure with memory "
		     "exhausted");
		Timeout thirtyticks{30};
		TEST(heap_allocate(&thirtyticks,
		                   MALLOC_CAPABILITY,
		                   BigAllocSize,
		                   AllocateWaitHeapFull) == nullptr,
		     "Time-limited blocking allocation did not return failure with "
		     "memory exhausted");
		TEST(thirtyticks.remaining == 0,
		     "Allocation with heap full wait flag set did not wait on memory "
		     "exhausted");
		debug_log("Checking that the 'quota exhausted' flag works");
		TEST(heap_allocate(&forever,
		                   EMPTY_HEAP,
		                   BigAllocSize,
		                   AllocateWaitRevocationNeeded) == nullptr,
		     "Blocking heap allocation with the quota exhausted flag unset did "
		     "not "
		     "return failure with memory "
		     "exhausted");
		thirtyticks = Timeout{30};
		TEST(heap_allocate(&thirtyticks,
		                   EMPTY_HEAP,
		                   BigAllocSize,
		                   AllocateWaitQuotaExceeded) == nullptr,
		     "Time-limited blocking allocation did not return failure with "
		     "memory exhausted");
		TEST(
		  thirtyticks.remaining == 0,
		  "Allocation with quota exhausted wait flag set did not wait on quota "
		  "exhausted");
		// Note: we do not test the functioning of
		// `AllocateWaitQuotaExceeded` as this would require to be able
		// to manipulate the quarantine to be reliably done.
		debug_log("Trying a huge allocation");
		// nullptr check because we explicitly want to check for OOM
		TEST(heap_allocate(&forever, MALLOC_CAPABILITY, 1024 * 1024 * 1024) ==
		       nullptr,
		     "Non-blocking heap allocation did not return failure on huge "
		     "allocation");
		// Wake up the thread that will free memory
		freeStart = 1;
		debug_log("Notifying deallocation thread to start with futex {}",
		          &freeStart);
		freeStart.notify_one();
		debug_log("Entering blocking malloc");
		Timeout t{AllocTimeout};
		void   *ptr = heap_allocate(&t, MALLOC_CAPABILITY, BigAllocSize);
		TEST(__builtin_cheri_tag_get(ptr),
		     "Failed to make progress on blocking allocation, allocation "
		     "returned {}",
		     ptr);
		free(ptr);
		// Wait until the background thread has freed everything.
		freeStart.wait(1);
		allocations.clear();
	}

	/**
	 * This test aims to exercise as many possibilities in the allocator as
	 * possible.
	 */
	void test_fuzz()
	{
		static constexpr size_t MaxAllocs    = 256;
		static constexpr size_t AllocSizes[] = {
		  16, 64, 72, 96, 128, 256, 384, 1024};
		static constexpr size_t NAllocSizes = std::size(AllocSizes);

		static constexpr size_t NCachedFrees = 4 * MaxAllocCount;

		ds::xoroshiro::P32R16 rand = {};
		auto                  t    = Timeout(0); /* don't sleep */

		std::vector<void *> cachedFrees;
		cachedFrees.resize(NCachedFrees);

		auto doAlloc = [&](size_t sz) {
			CHERI::Capability p{heap_allocate(&t, MALLOC_CAPABILITY, sz)};

			if (p.is_valid())
			{
				// dlmalloc can give you one granule more.
				TEST(p.length() == sz || p.length() == sz + 8,
				     "Bad return length");
				memset(p, 0xCA, sz);
				allocations.push_back(p);
			}
		};

		auto doFree = [&]() {
			size_t ix       = rand.next() % allocations.size();
			void  *p        = allocations[ix];
			allocations[ix] = allocations.back();
			allocations.pop_back();

			TEST(CHERI::Capability{p}.is_valid(), "Double free {}", p);

			cachedFrees[rand.next() % NCachedFrees] = p;

			free(p);
		};

		allocations.clear();
		allocations.reserve(MaxAllocs);

		for (size_t i = 0; i < 8 * TestIterations; ++i)
		{
			if ((i & 0x7) == 0)
			{
				/*
				 * Some notion of progress on the console is useful, but don't
				 * be too chatty.
				 */
				debug_log("fuzz i={}", i);
			}

			for (size_t j = rand.next() & 0xF; j > 0; j--)
			{
				if (allocations.size() < MaxAllocs)
				{
					size_t szix = rand.next() % NAllocSizes;
					size_t sz   = AllocSizes[szix];
					doAlloc(sz);
				}
			}

			for (size_t j = rand.next() & 0xF; j > 0; j--)
			{
				if (allocations.size() > 0)
				{
					doFree();
				}
			}

			for (void *p : cachedFrees)
			{
				TEST(!Capability{p}.is_valid(), "Detected necromancy: {}", p);
			}
		}

		cachedFrees.clear();

		for (auto allocation : allocations)
		{
			free(allocation);
		}
		allocations.clear();
	}

	void test_claims()
	{
		debug_log("Beginning tests on claims");
		auto quotaLeft = heap_quota_remaining(MALLOC_CAPABILITY);
		TEST(quotaLeft == MALLOC_QUOTA,
		     "After claim and free from {}-byte quota, {} bytes left before "
		     "running claims tests",
		     MALLOC_QUOTA,
		     quotaLeft);
		/*
		 * Allocate sufficiently small objects that no additional alignment or
		 * padding requirements arise from capability encoding.  Nevertheless,
		 * because our allocator imposes minimal chunk sizes, we cannot be sure
		 * that the underlying object is exactly this size.  When probing,
		 * permit some slack, up to CHERIOTHeapMinChunkSize.
		 */
		size_t allocSize       = 128;
		auto   mallocQuotaLeft = heap_quota_remaining(MALLOC_CAPABILITY);
		CHERI::Capability alloc{
		  heap_allocate(&noWait, MALLOC_CAPABILITY, allocSize)};
		TEST(alloc.is_valid(), "Allocation failed");
		int  claimCount = 0;
		auto claim      = [&]() {
            ssize_t claimSize = heap_claim(SECOND_HEAP, alloc);
            claimCount++;
            TEST((allocSize <= claimSize),
                 "{}-byte allocation claimed as {} bytes (claim number {})",
                 allocSize,
                 claimSize,
                 claimCount);
		};
		claim();
		int ret = heap_free(SECOND_HEAP, alloc);
		TEST(ret == 0, "Freeing claimed allocation returned {}", ret);
		quotaLeft = heap_quota_remaining(SECOND_HEAP);
		TEST(quotaLeft == SECOND_HEAP_QUOTA,
		     "After claim and free from {}-byte quota, {} bytes left",
		     SECOND_HEAP_QUOTA,
		     quotaLeft);
		claim();
		quotaLeft = heap_quota_remaining(SECOND_HEAP);
		claim();
		auto quotaLeftAfterSecondClaim = heap_quota_remaining(SECOND_HEAP);
		TEST(quotaLeft == quotaLeftAfterSecondClaim,
		     "Claiming twice reduced quota from {} to {}",
		     quotaLeft,
		     quotaLeftAfterSecondClaim);
		debug_log("Freeing object on malloc capability: {}", alloc);
		ret = heap_free(MALLOC_CAPABILITY, alloc);
		TEST(ret == 0, "Failed to free claimed object, return: {}", ret);
		auto mallocQuota2 = heap_quota_remaining(MALLOC_CAPABILITY);
		TEST(mallocQuotaLeft == mallocQuota2,
		     "Freeing claimed object did not restore quota to {}, quota is {}",
		     mallocQuotaLeft,
		     mallocQuota2);
		ret = heap_free(SECOND_HEAP, alloc);
		TEST(ret == 0, "Freeing claimed allocation returned {}", ret);
		ret = heap_free(SECOND_HEAP, alloc);
		TEST(ret == 0, "Freeing claimed (twice) allocation returned {}", ret);
		quotaLeft = heap_quota_remaining(SECOND_HEAP);
		TEST(quotaLeft == 1024,
		     "After claim and free twice from 1024-byte quota, {} bytes left",
		     quotaLeft);
		TEST(!__builtin_launder(&alloc)->is_valid(),
		     "Heap capability still valid after releasing last claim: {}",
		     alloc);
	}

	/**
	 * Test heap_free_all.  Make sure that we can reclaim all memory associated
	 * with a single quota.
	 */
	void test_free_all()
	{
		ssize_t allocated = 0;
		debug_log("Quota left before allocating: {}",
		          heap_quota_remaining(SECOND_HEAP));
		// Allocate and leak some things:
		for (size_t i = 16; i < 256; i <<= 1)
		{
			allocated += i;
			TEST(
			  __builtin_cheri_tag_get(heap_allocate(&noWait, SECOND_HEAP, i)),
			  "Allocating {} bytes failed",
			  i);
		}
		debug_log("Quota left after allocating {} bytes: {}",
		          allocated,
		          heap_quota_remaining(SECOND_HEAP));
		int freed = heap_free_all(SECOND_HEAP);
		// We can free more than we think the requested size doesn't include
		// object headers.
		TEST(freed > allocated,
		     "Allocated {} bytes but heap_free_all freed {}",
		     allocated,
		     freed);
		auto quotaLeft = heap_quota_remaining(SECOND_HEAP);
		TEST(quotaLeft == SECOND_HEAP_QUOTA,
		     "After alloc and free from {}-byte quota, {} bytes left",
		     SECOND_HEAP_QUOTA,
		     quotaLeft);
	}

	void test_hazards()
	{
		int sleeps;
		debug_log("Before allocating, quota left: {}",
		          heap_quota_remaining(SECOND_HEAP));
		Timeout longTimeout{1000};
		size_t  allocSize = 16;
		void   *ptr       = heap_allocate(&longTimeout, SECOND_HEAP, allocSize);
		TEST(__builtin_cheri_tag_get(ptr),
		     "Failed to allocate {} bytes",
		     allocSize);
		void *ptr2 = heap_allocate(&longTimeout, SECOND_HEAP, allocSize);
		TEST(__builtin_cheri_tag_get(ptr2),
		     "Failed to allocate {} bytes",
		     allocSize);
		debug_log("After allocating, quota left: {}",
		          heap_quota_remaining(SECOND_HEAP));
		static cheriot::atomic<int> state = 0;
		async([=]() {
			Timeout t{1};
			int     claimed = heap_claim_ephemeral(&t, ptr, ptr2);
			TEST(claimed == 0, "Heap claim failed: {}", claimed);
			state = 1;
			while (state.load() == 1)
			{
				// This is not a cross-compartment call and so
				// won't drop the ephemeral claims.
				yield();
			}
			debug_log("Releasing hazard pointers");
		});
		// Allow the async function to run and establish hazards
		sleeps = 0;
		while (state.load() != 1)
		{
			TEST(sleep(1) >= 0, "Failed to sleep");
			TEST(sleeps++ < 100,
			     "Background thread failed to establish hazards");
		}
		debug_log("Before freeing, quota left: {}",
		          heap_quota_remaining(SECOND_HEAP));
		TEST_EQUAL(heap_free(SECOND_HEAP, ptr), 0, "First free failed");
		debug_log("After free 1, quota left: {}",
		          heap_quota_remaining(SECOND_HEAP));
		TEST_EQUAL(heap_free(SECOND_HEAP, ptr2), 0, "Second free failed");
		debug_log("After free 2, quota left: {}",
		          heap_quota_remaining(SECOND_HEAP));
		TEST(Capability{ptr}.is_valid_temporal(),
		     "Pointer in hazard slot was freed: {}",
		     ptr);
		TEST(Capability{ptr2}.is_valid_temporal(),
		     "Pointer in hazard slot was freed: {}",
		     ptr2);
		// Try a double free. This should fail with `-EPERM` (since the
		// chunks are still valid allocations) and not affect our
		// quota.
		TEST_EQUAL(heap_free(SECOND_HEAP, ptr),
		           -EPERM,
		           "Attempt to free freed but hazarded pointer not EPERM");
		auto quotaLeft = heap_quota_remaining(SECOND_HEAP);
		TEST(quotaLeft == SECOND_HEAP_QUOTA,
		     "After alloc and free from {}-byte quota, {} bytes left",
		     SECOND_HEAP_QUOTA,
		     quotaLeft);
		// Try to claim the buffer. This should succeed since there is
		// still an ephemeral claim on it.
		ssize_t claimSize = heap_claim(MALLOC_CAPABILITY, ptr2);
		TEST((allocSize <= claimSize),
		     "{}-byte allocation claimed as {} bytes",
		     allocSize,
		     claimSize);
		// Stop the async and sleep until it is gone. When the async is
		// freed, the allocator is called, causing it to release the
		// ephemeral claims and truly free `ptr`. At this stage calling
		// `heap_free` on `ptr` will now return `-EINVAL` (instead of
		// `-EPERM`).
		state  = 2;
		sleeps = 0;
		while (heap_free(SECOND_HEAP, ptr) != -EINVAL)
		{
			TEST(sleep(1) >= 0, "Failed to sleep");
			TEST(sleeps++ < 100,
			     "Sleeping for too long waiting for async lambda to be freed");
		}
		// The double frees above should not affect our quota.
		quotaLeft = heap_quota_remaining(SECOND_HEAP);
		TEST(quotaLeft == SECOND_HEAP_QUOTA,
		     "Double-frees changed {}-byte quota to {} bytes",
		     SECOND_HEAP_QUOTA,
		     quotaLeft);
		// Now `ptr` should be invalid, but `ptr2` should remain valid
		// since it still has a claim on it.
		TEST(!Capability{ptr}.is_valid_temporal(),
		     "Pointer was not freed: {}",
		     ptr);
		TEST(
		  Capability{ptr2}.is_valid_temporal(), "Pointer was freed: {}", ptr2);
		// Release the claim on `ptr2`.
		debug_log("Before freeing, quota left: {}",
		          heap_quota_remaining(MALLOC_CAPABILITY));
		TEST_EQUAL(
		  heap_free(MALLOC_CAPABILITY, ptr2), 0, "Releasing the claim failed");
		debug_log("After releasing the claim, quota left: {}",
		          heap_quota_remaining(MALLOC_CAPABILITY));
		// Now the allocation should be invalid and the quota refunded.
		TEST(!Capability{ptr2}.is_valid_temporal(),
		     "Pointer in hazard slot was freed: {}",
		     ptr);
		quotaLeft = heap_quota_remaining(MALLOC_CAPABILITY);
		TEST(quotaLeft == MALLOC_QUOTA,
		     "The concurrent claim changed {}-byte quota to {} bytes",
		     MALLOC_QUOTA,
		     quotaLeft);
		debug_log("Hazard pointer tests done");
	}

	void test_large_token(size_t tokenSize)
	{
		void      *unsealedCapability;
		auto       sealingCapability = STATIC_SEALING_TYPE(sealingTest);
		Timeout    t{AllocTimeout};
		Capability sealedPointer =
		  token_sealed_unsealed_alloc(&t,
		                              MALLOC_CAPABILITY,
		                              sealingCapability,
		                              tokenSize,
		                              &unsealedCapability);
		TEST(sealedPointer.is_valid(),
		     "Failed to allocate large sealed capability that requires padding "
		     "for the header");
		TEST(sealedPointer.is_sealed(), "Failed to allocate sealed capability");
		TEST(!Capability{unsealedCapability}.is_sealed(),
		     "Failed to allocate sealed capability");
		size_t unsealedLength = Capability{unsealedCapability}.length();
		TEST(unsealedLength >= tokenSize,
		     "Length of unsealed capability is not {}: {}",
		     tokenSize,
		     unsealedCapability);
		TEST(sealedPointer.length() >= unsealedLength + 8,
		     "Length of unsealed capability is not the unsealed size plus the "
		     "header size: {}",
		     unsealedCapability);
		TEST(sealedPointer.address() + 4 <
		       Capability{unsealedCapability}.address(),
		     "Header for the sealed capability ({}) is not before the start of "
		     "the unsealed capability ({})",
		     sealedPointer,
		     unsealedCapability);
		Capability unsealedLarge =
		  token_unseal(sealingCapability, Sealed<void>{sealedPointer.get()});
		TEST(unsealedLarge == Capability{unsealedCapability},
		     "Unsealing large capability gave a different capability to the "
		     "expected one ({} != {})",
		     unsealedLarge,
		     unsealedCapability);
		int destroyed = token_obj_destroy(
		  MALLOC_CAPABILITY, sealingCapability, sealedPointer);
		TEST(destroyed == 0, "Failed to destroy large sealed capability");
	}

	/**
	 * Test the sealing APIs.  Tests that we can allocate and free sealed
	 * objects, that we can't collect them with the wrong capabilities, and that
	 * they don't go away when heap_free_all is called.
	 *
	 * This is marked as noinline because otherwise the predict-false on the
	 * test failures causes all of the log-message-and-fail blocks to be moved
	 * to the end of the function and, if this is inlined, ends up with some
	 * branches that are more than 2 KiB away from their targets.
	 */
	__noinline void test_token()
	{
		debug_log("Testing token allocation");
		size_t validSizes[] = {
		  128, 0xfe9, 8193, 511, 511 << 1, 511 << 2, 511 << 3};
		for (size_t tokenSize : validSizes)
		{
			debug_log("Testing (expected valid) token size {}", tokenSize);
			test_large_token(tokenSize);
		}
		size_t invalidSizes[] = {0, 0xffffffff};
		for (size_t tokenSize : invalidSizes)
		{
			debug_log("Testing (expected invalid) token size {}", tokenSize);
			auto       sealingCapability = STATIC_SEALING_TYPE(sealingTest);
			void      *unsealedCapability;
			Capability sealedPointer =
			  token_sealed_unsealed_alloc(&noWait,
			                              MALLOC_CAPABILITY,
			                              sealingCapability,
			                              tokenSize,
			                              &unsealedCapability);
			TEST(!sealedPointer.is_valid(),
			     "Allocated {} for invalid token size {}",
			     sealedPointer,
			     tokenSize);
			TEST(!Capability{unsealedCapability}.is_valid(),
			     "Allocated {} for invalid token size {}",
			     unsealedCapability,
			     tokenSize);
		}
		auto    sealingCapability = STATIC_SEALING_TYPE(sealingTest);
		Timeout noWait{0};
		void   *unsealedCapability;
		TEST(heap_quota_remaining(SECOND_HEAP) == SECOND_HEAP_QUOTA,
		     "Quota left before allocating is {}, expected {}",
		     heap_quota_remaining(SECOND_HEAP),
		     SECOND_HEAP_QUOTA);
		Capability sealedPointer = token_sealed_unsealed_alloc(
		  &noWait, SECOND_HEAP, sealingCapability, 128, &unsealedCapability);
		TEST(sealedPointer.is_valid(), "Failed to allocate capability");
		TEST(sealedPointer.is_sealed(), "Failed to allocate sealed capability");
		TEST(!Capability{unsealedCapability}.is_sealed(),
		     "Failed to allocate sealed capability");

		int canFree =
		  token_obj_can_destroy(SECOND_HEAP, sealingCapability, sealedPointer);
		TEST(canFree == 0,
		     "Should be able to free a sealed heap capability with the correct "
		     "pair of capabilities but failed with {}",
		     canFree);
		canFree = token_obj_can_destroy(
		  MALLOC_CAPABILITY, sealingCapability, sealedPointer);
		TEST(canFree != 0,
		     "Should not be able to free a sealed capability with the wrong "
		     "malloc capability but succeeded");
		canFree = token_obj_can_destroy(
		  SECOND_HEAP, STATIC_SEALING_TYPE(wrongSealingKey), sealedPointer);
		TEST(canFree != 0,
		     "Should not be able to free a sealed capability with the wrong "
		     "sealing key but succeeded");

		TEST(token_obj_destroy(
		       MALLOC_CAPABILITY, sealingCapability, sealedPointer) != 0,
		     "Freeing a sealed capability with the wrong allocator succeeded");
		debug_log("Before heap free all, quota left: {}",
		          heap_quota_remaining(SECOND_HEAP));
		int ret = heap_free_all(SECOND_HEAP);
		debug_log("After heap free all, quota left: {}",
		          heap_quota_remaining(SECOND_HEAP));
		TEST(sealedPointer.is_valid(),
		     "heap_free_all freed sealed capability: {}",
		     sealedPointer);
		TEST(ret == 0, "heap_free_all returned {}, expected 0", ret);
		TEST(
		  token_obj_destroy(SECOND_HEAP, sealingCapability, sealedPointer) == 0,
		  "Freeing a sealed capability with the correct capabilities failed");
		TEST(heap_quota_remaining(SECOND_HEAP) == SECOND_HEAP_QUOTA,
		     "Quota left after allocating sealed objects and cleaning up is "
		     "{}, expected {}",
		     heap_quota_remaining(SECOND_HEAP),
		     SECOND_HEAP_QUOTA);

		/*
		 * Test the bad-outparam failure path of token_sealed_unsealled_alloc.
		 * This is expected to still return the allocated object.
		 */
		sealedPointer = token_sealed_unsealed_alloc(
		  &noWait, SECOND_HEAP, sealingCapability, 16, nullptr);
		TEST(sealedPointer.is_valid(), "Invalid outparam case failed alloc");
		TEST_EQUAL(
		  token_obj_destroy(SECOND_HEAP, sealingCapability, sealedPointer),
		  0,
		  "Failed to free invalid-outparam sealed object");
		TEST_EQUAL(heap_quota_remaining(SECOND_HEAP),
		           SECOND_HEAP_QUOTA,
		           "Invalid outparam path failed to restore quota");
	}

} // namespace

/**
 * Allocator test entry point.
 */
int test_allocator()
{
	GlobalConstructors::run();

	const ptraddr_t HeapStart = LA_ABS(__export_mem_heap);
	const ptraddr_t HeapEnd   = LA_ABS(__export_mem_heap_end);

	const size_t HeapSize = HeapEnd - HeapStart;
	debug_log("Heap size is {} bytes", HeapSize);

	test_preflight();

	// Make sure that free() accepts delegated (non-global) pointers.
	{
		Timeout t{UnlimitedTimeout};
		void *volatile p = heap_allocate(&t, MALLOC_CAPABILITY, 32);
		TEST(Capability{p}.is_valid(),
		     "Could not perform an early allocation: {}",
		     p);

		Capability q = {p};
		q.without_permissions(CHERI::Permission::Global);

		TEST_SUCCESS(heap_free(MALLOC_CAPABILITY, q));
		TEST(!Capability{p}.is_valid_temporal(),
		     "Free of non-global pointer failed to revoke");
	}

	test_token();
	test_hazards();

	// Make sure that free works only on memory owned by the caller.
	Timeout t{5};
	test_free_all();
	void *ptr = heap_allocate(&t, STATIC_SEALED_VALUE(secondHeap), 32);
	TEST(__builtin_cheri_tag_get(ptr), "Failed to allocate 32 bytes");
	TEST(heap_address_is_valid(ptr) == true,
	     "Heap object incorrectly reported as not heap address");
	int ret = heap_free(MALLOC_CAPABILITY, ptr);
	TEST(
	  ret == -EPERM,
	  "Heap free with the wrong capability returned {}. expected -EPERM ({})",
	  ret,
	  -EPERM);
	ret = heap_free(STATIC_SEALED_VALUE(secondHeap), ptr);
	TEST(ret == 0,
	     "Heap free with the correct capability returned failed with {}.",
	     ret);
	auto quotaLeft = heap_quota_remaining(STATIC_SEALED_VALUE(secondHeap));
	TEST(quotaLeft == 1024,
	     "After alloc and free from 1024-byte quota, {} bytes left",
	     quotaLeft);
	test_claims();

	TEST(heap_address_is_valid(&t) == false,
	     "Stack object incorrectly reported as heap address");
	TEST(heap_address_is_valid(&noWait) == false,
	     "Global object incorrectly reported as heap address");

	t = 5;
	Capability array{heap_allocate_array(&t, MALLOC_CAPABILITY, 0x80000004, 2)};
	TEST(
	  !array.is_valid(), "Allocating too large an array succeeded: {}", array);
	array = heap_allocate_array(&t, MALLOC_CAPABILITY, 16, 2);
	TEST(array.is_valid(), "Allocating array failed: {}", array);
	// If there's heap fragmentation, this may be rounded up to 40 because we
	// can't use the 8 bytes after the end for another object.
	TEST((array.length() >= 32) && (array.length() <= 40),
	     "Allocating array returned incorrect length: {}",
	     array);
	ret = heap_free(MALLOC_CAPABILITY, array);
	TEST(ret == 0, "Freeing array failed: {}", ret);

	test_blocking_allocator(HeapSize);
	TEST_SUCCESS(heap_quarantine_empty());
	test_revoke(HeapSize);
	test_fuzz();
	allocations.clear();
	allocations.shrink_to_fit();
	quotaLeft = heap_quota_remaining(MALLOC_CAPABILITY);
	TEST(quotaLeft == MALLOC_QUOTA,
	     "After alloc and free from 0x100000-byte quota, {} bytes left",
	     quotaLeft);
	return 0;
}

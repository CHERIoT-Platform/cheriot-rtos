// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Use a large quota for this compartment.
#define MALLOC_QUOTA 0x100000
#define TEST_NAME "Allocator"
#include "tests.hh"
#include <debug.hh>
#include <ds/xoroshiro.h>
#include <errno.h>
#include <futex.h>
#include <global_constructors.hh>
#include <thread.h>
#include <thread_pool.h>
#include <vector>

using thread_pool::async;
DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(secondHeap, 1024);

namespace
{
	/**
	 * Maximum timeout for a blocking malloc.  This needs to be large enough
	 * that we can do a complete revocation sweep in this many ticks but small
	 * enough that we don't cause CI to block forever.
	 */
	constexpr size_t AllocTimeout = 1 << 8;

	Timeout noWait{0};

	/**
	 * Size of an allocation that is big enough that we'll exhaust memory before
	 * we allocate `MaxAllocCount` of them.
	 */
	constexpr size_t BigAllocSize  = 1024 * 16;
	constexpr size_t AllocSize     = 0xff0;
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
	 * Test the revoker by constantly allocating and freeing batches of
	 * allocations. The total amount of allocations must greatly exceed the heap
	 * size to force a constant stream of allocation failures and revocations.
	 * The time required to finish the test indicates revoker performance, lower
	 * the better.
	 *
	 * This performance test should not fail. If it fails it's either the
	 * allocations in one iteration exceed the total heap size, or the revoker
	 * is buggy or too slow.
	 */
	void test_revoke()
	{
		allocations.resize(MaxAllocCount);
		for (size_t i = 0; i < TestIterations; ++i)
		{
			for (auto &allocation : allocations)
			{
				Timeout t{AllocTimeout};
				allocation = heap_allocate(&t, MALLOC_CAPABILITY, AllocSize);
				TEST(
				  allocation != nullptr,
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
				  __builtin_cheri_tag_get(allocation) == 0,
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
			Timeout t{1};
			thread_sleep(&t);
		}
		allocations.clear();
	}

	uint32_t freeStart;
	/**
	 * Test that we can do a long-running blocking allocation in one thread and
	 * a free in another thread and make forward progress.
	 */
	void test_blocking_allocator()
	{
		allocations.resize(MaxAllocCount);
		// Create the background worker before we try to exhaust memory.
		async([]() {
			// Make sure that we reach the blocking free.
			debug_log("Deallocation thread sleeping");
			futex_wait(&freeStart, 0);
			// One extra sleep to make sure that we're really in the blocking
			// sleep.
			Timeout t{1};
			thread_sleep(&t);
			debug_log(
			  "Deallocation thread resuming, freeing pool of allocations");
			// Free all of the allocations to make space.
			for (auto &allocation : allocations)
			{
				if (allocation != nullptr)
				{
					heap_free(MALLOC_CAPABILITY, allocation);
				}
			}
			// Notify the parent thread that we're done.
			freeStart = 2;
			futex_wake(&freeStart, 1);
		});

		bool memoryExhausted = false;
		for (auto &allocation : allocations)
		{
			Timeout t{0};
			allocation = heap_allocate(&noWait, MALLOC_CAPABILITY, 1024 * 16);
			if (allocation == nullptr)
			{
				memoryExhausted = true;
				break;
			}
		}
		TEST(memoryExhausted, "Failed to exhaust memory");
		debug_log("Trying a non-blocking allocation");
		TEST(heap_allocate(&noWait, MALLOC_CAPABILITY, 1024 * 16) == nullptr,
		     "Non-blocking heap allocation did not return failure with memory "
		     "exhausted");
		debug_log("Trying a huge allocation");
		Timeout forever{UnlimitedTimeout};
		TEST(heap_allocate(&forever, MALLOC_CAPABILITY, 1024 * 1024 * 1024) ==
		       nullptr,
		     "Non-blocking heap allocation did not return failure on huge "
		     "allocation");
		// Wake up the thread that will free memory
		freeStart = 1;
		futex_wake(&freeStart, 1);
		debug_log("Entering blocking malloc");
		Timeout t{AllocTimeout};
		void   *ptr = heap_allocate(&t, MALLOC_CAPABILITY, 1024 * 16);
		TEST(ptr != nullptr,
		     "Failed to make progress on blocking allocation, allocation "
		     "returned {}",
		     ptr);
		free(ptr);
		// Wait until the background thread has freed everything.
		futex_wait(&freeStart, 1);
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

		ds::xoroshiro::P32R16 rand = {};
		auto                  t    = Timeout(0); /* don't sleep */

		auto doAlloc = [&](size_t sz) {
			auto p = heap_allocate(&t, MALLOC_CAPABILITY, sz);

			if (p != nullptr)
			{
				CHERI::Capability pwrap{p};
				// dlmalloc can give you one granule more.
				TEST(pwrap.length() == sz || pwrap.length() == sz + 8,
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
		}

		for (auto allocation : allocations)
		{
			free(allocation);
		}
		allocations.clear();
	}

} // namespace

/**
 * Allocator test entry point.
 */
void test_allocator()
{
	GlobalConstructors::run();

	// Make sure that free works only on memory owned by the caller.
	Timeout t{5};
	void   *ptr = heap_allocate(&t, STATIC_SEALED_VALUE(secondHeap), 32);
	TEST(ptr, "Failed to allocate 32 bytes");
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
	size_t quotaLeft = heap_quota_remaining(STATIC_SEALED_VALUE(secondHeap));
	TEST(quotaLeft == 1024,
	     "After alloc and free from 1024-byte quota, {} bytes left",
	     quotaLeft);

	test_blocking_allocator();
	test_revoke();
	test_fuzz();
	allocations.clear();
	allocations.shrink_to_fit();
	quotaLeft = heap_quota_remaining(MALLOC_CAPABILITY);
	TEST(quotaLeft == 0x100000,
	     "After alloc and free from 0x100000-byte quota, {} bytes left",
	     quotaLeft);
}

#include "../timing.h"
#include <allocator.h>
#include <compartment.h>
#include <debug.hh>
#include <locks.hh>

using Debug = ConditionalDebug<DEBUG_ALLOCBENCH, "Allocator benchmark">;

void run(size_t allocSize, size_t allocations, size_t inHeap)
{
	auto start = rdcycle();
	for (size_t i = 0; i < allocations; i++)
	{
		void *ptr = malloc(allocSize);
		Debug::Assert(ptr != nullptr,
		              "Allocation {} of {} {}-byte allocations failed.",
		              i,
		              allocations,
		              allocSize);
		free(ptr);
	}
	auto end = rdcycle();

	asm volatile("" : : : "memory");

	printf(__XSTRING(BOARD) "\t%ld\t%ld\t%ld\t%ld\n",
	       static_cast<int>(allocSize),
	       allocations,
	       inHeap,
	       end - start);

	auto quota = heap_quota_remaining(MALLOC_CAPABILITY);
	Debug::Invariant(quota == MALLOC_QUOTA,
	                 "Quota remaining {}, should be {}",
	                 quota,
	                 MALLOC_QUOTA);

	Debug::Invariant(heap_quarantine_empty() == 0,
	                 "Call to heap_quarantine_empty failed");
}

/**
 * Try allocating 1 MiB of memory in allocation sizes ranging from 32 - 4096
 * bytes, report how long it takes.
 */
int __cheri_compartment("allocbench") run()
{
	const ptraddr_t HeapStart = LA_ABS(__export_mem_heap);
	const ptraddr_t HeapEnd   = LA_ABS(__export_mem_heap_end);

	const size_t HeapSize = HeapEnd - HeapStart;

	const size_t TotalSize = 8 * HeapSize;

	auto runWrap = [&](size_t allocSize) {
		return run(allocSize, TotalSize / allocSize, HeapSize / allocSize);
	};

	// Make sure sail doesn't print annoying log messages in the middle of the
	// output the first time that allocation happens.
	free(malloc(16));

	Debug::Invariant(heap_quarantine_empty() == 0,
	                 "Call to heap_quarantine_empty failed");

	printf("#board\tsize\tnalloc\tinheap\ttime\n");

	const size_t MinimumSize = 32;

	for (size_t pow2 = MinimumSize; pow2 <= HeapSize; pow2 <<= 1)
	{
		if (pow2 < (HeapSize / 4))
		{
			runWrap(pow2);
		}
		else
		{
			for (size_t mantissa = 0; mantissa < 7; mantissa++)
			{
				size_t size = pow2 + mantissa * (pow2 / 8);
				if (size < HeapSize)
				{
					// These tend to have few allocations; run the test
					// repeatedly.
					for (size_t i = 0; i < 4; i++)
					{
						runWrap(size);
					}
				}
			}
		}
	}

	printf("----- end of results (HeapSize is %zd)\n", HeapSize);

	return 0;
}

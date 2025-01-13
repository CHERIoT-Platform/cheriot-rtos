#include "../timing.h"
#include <compartment.h>
#include <debug.hh>
#include <locks.hh>

using Debug = ConditionalDebug<DEBUG_ALLOCBENCH, "Allocator benchmark">;

/**
 * Try allocating 1 MiB of memory in allocation sizes ranging from 32 - 4096
 * bytes, report how long it takes.
 */
int __cheri_compartment("allocbench") run()
{
	// Make sure sail doesn't print annoying log messages in the middle of the
	// output the first time that allocation happens.
	free(malloc(16));
	Debug::Invariant(heap_quarantine_empty() == 0,
	                 "Call to heap_quarantine_empty failed");
	printf("#board\tsize\ttime\n");
	const size_t MinimumSize = 32;
	const size_t MaximumSize = 131072;
	const size_t TotalSize   = 1024 * 1024;
	for (size_t size = MinimumSize; size <= MaximumSize; size <<= 1)
	{
		size_t allocations = TotalSize / size;
		auto   start       = rdcycle();
		for (size_t i = 0; i < allocations; i++)
		{
			void *ptr = malloc(size);
			Debug::Assert(ptr != nullptr, "Allocation {} of {} {}-byte allocations failed.", i, allocations, size);
			free(ptr);
		}
		auto end = rdcycle();
		printf(__XSTRING(BOARD) "\t%ld\t%ld\n", static_cast<int>(size), end - start);
		auto quota = heap_quota_remaining(MALLOC_CAPABILITY);
		Debug::Invariant(quota == MALLOC_QUOTA, "Quota remaining {}, should be {}", quota, MALLOC_QUOTA);
		Debug::log("Flushing quarantine");
		Debug::Invariant(heap_quarantine_empty() != -ECOMPARTMENTFAIL,
		                 "Call to heap_quarantine_empty failed");
		Debug::log("Flushed quarantine");
	}

	return 0;
}

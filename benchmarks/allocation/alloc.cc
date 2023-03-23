#include "../timing.h"
#include <compartment.h>
#include <debug.hh>
#include <locks.hh>

using Debug = ConditionalDebug<DEBUG_ALLOCBENCH, "Allocator benchmark">;

/**
 * Try allocating 1 MiB of memory in allocation sizes ranging from 32 - 4096
 * bytes, report how long it takes.
 */
void __cheri_compartment("allocbench") run()
{
	MessageBuilder<ImplicitUARTOutput> out;
	out.format("#board\tsize\ttime");
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
		out.format(__XSTRING(BOARD) "\t{}\t{}\n", static_cast<int>(size), end - start);
		size_t quota = heap_quota_remaining(MALLOC_CAPABILITY);
		Debug::Invariant(quota == MALLOC_QUOTA, "Quota remaining {}, should be {}", quota, MALLOC_QUOTA);
		Debug::log("Flushing quarantine");
		heap_quarantine_empty();
		Debug::log("Flushed quarantine");
	}
}

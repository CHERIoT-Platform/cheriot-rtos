#include "../timing.h"
#include "callee.h"
#include <compartment.h>
#include <debug.hh>
#include <locks.hh>
#include <stdio.h>
#include <vector>

using Debug = ConditionalDebug<DEBUG_CALLER, "Compartment call benchmark">;

int __cheri_compartment("caller") run()
{
	static std::array<std::tuple<size_t, int, int, int>, 5> results;
	static int                                              nextResult = 0;
	static TicketLock                                       lock;
	static bool                                             headerWritten;
	size_t stackSize = get_stack_size();
	// Make sure that we hit the lock last if we have the biggest stack.  We
	// don't have enough stack space for the log call on the smallest stacks.
	if (stackSize == 0x1000)
	{
		Timeout timeout{100};
		Debug::Invariant(thread_sleep(&timeout) >= 0,
		                 "Compartment call thread_sleep failed");
	}
	LockGuard g{lock};
	auto [full, callPath, returnPath] = CHERI::with_interrupts_disabled([&]() {
		auto start  = rdcycle();
		auto middle = noop_return_rdcycle();
		auto end    = rdcycle();
		return std::tuple{end - start, middle - start, end - middle};
	});
	results[nextResult++]             = {stackSize, full, callPath, returnPath};
	if (stackSize == 0x1000)
	{
		printf("#board\tstack size\tfull call\tcall\treturn\n");
		for (auto [stackSize, full, callPath, returnPath] : results)
		{
			printf(__XSTRING(BOARD) "\t%d\t%d\t%d\t%d\n",
			       stackSize,
			       full,
			       callPath,
			       returnPath);
		}
	}

	return 0;
}

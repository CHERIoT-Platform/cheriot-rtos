#include "../timing.h"
#include "callee.h"
#include <compartment.h>
#include <debug.hh>
#include <locks.hh>
#include <stdio.h>
#include <vector>

using Debug = ConditionalDebug<DEBUG_CALLER, "Compartment call benchmark">;

__noinline int local_noop_return_metric()
{
	return METRIC();
}

int __cheri_compartment("caller") run()
{
	static std::array<std::tuple<size_t, int, int, int>, 25> results;
	static int                                               nextResult = 0;
	static TicketLock                                        lock;
	static bool                                              headerWritten;
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
	for (int i = 0; i < 5; i++)
	{
		auto [full, callPath, returnPath] =
		  CHERI::with_interrupts_disabled([&]() {
			  auto start  = METRIC();
			  auto middle = noop_return_metric();
			  auto end    = METRIC();
			  return std::tuple{end - start, middle - start, end - middle};
		  });
		results[nextResult++] = {stackSize, full, callPath, returnPath};
	}
	if (stackSize == 0x1000)
	{
		printf("---- Results using METRIC: " __XSTRING(METRIC) "\n");

		printf("#board\tstack size\tfull call\tcall\treturn\n");
		for (auto [stackSize, full, callPath, returnPath] : results)
		{
			printf(__XSTRING(BOARD) "\t%d\t%d\t%d\t%d\n",
			       stackSize,
			       full,
			       callPath,
			       returnPath);
		}

		printf("#board\tcaller stack\tcallee stack\tfull call\tcall\treturn\n");
		for (int callerUse = 128; callerUse <= 1024; callerUse <<= 1)
		{
			for (int calleeUse = 128; calleeUse <= 1024; calleeUse <<= 1)
			{
				for (int i = 0; i < 5; i++)
				{
					use_stack(callerUse);

					auto start  = METRIC();
					auto middle = stack_using_return_metric(calleeUse);
					auto end    = METRIC();

					printf(__XSTRING(BOARD) "\t%d\t%d\t%d\t%d\t%d\n",
					       callerUse,
					       calleeUse,
					       end - start,
					       middle - start,
					       end - middle);
				}
			}
		}

		printf("#function call\n");
		for (int i = 0; i < 5; i++)
		{
			auto start  = METRIC();
			auto middle = local_noop_return_metric();
			auto end    = METRIC();

			printf(__XSTRING(BOARD) "\t%d\t%d\t%d\n",
			       end - start,
			       middle - start,
			       end - middle);
		}

		printf("#library call\n");
		for (int i = 0; i < 5; i++)
		{
			auto start  = METRIC();
			auto middle = lib_noop_return_metric();
			auto end    = METRIC();

			printf(__XSTRING(BOARD) "\t%d\t%d\t%d\n",
			       end - start,
			       middle - start,
			       end - middle);
		}
	}

	return 0;
}

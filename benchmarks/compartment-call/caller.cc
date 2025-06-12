#include "../timing.h"
#include "callee.h"
#include <compartment.h>
#include <debug.hh>
#include <locks.hh>
#include <stdio.h>
#include <token.h>
#include <vector>

using Debug = ConditionalDebug<DEBUG_CALLER, "Compartment call benchmark">;

__noinline int local_noop_return_metric()
{
	return METRIC();
}

__noinline int local_attenuate_return_metric(void *p)
{
	using namespace CHERI;
	Capability cap{p};
	cap.permissions() &= Permission::Load;
	cap.bounds() = 1;

	return cap.is_valid() ? METRIC() : 0;
}

auto benchWithMiddle = []<typename F>(F f) {
	printf("#board\tfull call\tcall\treturn\n");
	for (int i = 0; i < 5; i++)
	{
		auto start  = METRIC();
		auto middle = f(i);
		auto end    = METRIC();

		printf(__XSTRING(BOARD) "\t%d\t%d\t%d\n",
		       end - start,
		       middle - start,
		       end - middle);
	}
};

auto benchAround = []<typename F>(F f) {
	printf("#board\tfull call\n");
	for (int i = 0; i < 5; i++)
	{
		auto start = METRIC();
		f(i);
		auto end = METRIC();

		printf(__XSTRING(BOARD) "\t%d\n", end - start);
	}
};

int __cheri_compartment("caller") run()
{
	static std::array<std::tuple<size_t, int, int, int>, 20> results;
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
			  auto middle = local_noop_return_metric();
			  auto end    = METRIC();
			  return std::tuple{end - start, middle - start, end - middle};
		  });
		results[nextResult++] = {stackSize, full, callPath, returnPath};
	}
	if (stackSize == 0x1000)
	{
		printf("---- Results using METRIC: " __XSTRING(METRIC) "\n");

		printf("#no-op cross-calls\n");
		printf("#board\tstack size\tfull call\tcall\treturn\n");
		for (auto [stackSize, full, callPath, returnPath] : results)
		{
			printf(__XSTRING(BOARD) "\t%d\t%d\t%d\t%d\n",
			       stackSize,
			       full,
			       callPath,
			       returnPath);
		}

		printf("#cross call with check_pointer\n");
		benchWithMiddle(
		  [](int) { return callee_check_pointer_return_metric(&nextResult); });

		printf("#cross call with heap_claim_ephemeral\n");
		benchWithMiddle([](int) {
			return callee_ephemeral_claim_return_metric(&nextResult);
		});

		/*
		 * This marks the first time the callee uses its MALLOC_QUOTA, so the
		 * first one of these is going to be very slightly more expensive as the
		 * allocator assigns a new identifier.
		 */
		printf("#cross call with claim_release\n");
		benchWithMiddle(
		  [](int) { return callee_claim_release_return_metric(&nextResult); });

		printf("#cross call dereference OK\n");
		benchAround([](int) { return callee_dereference(&nextResult); });

		printf("#cross call dereference nullptr\n");
		benchAround([](int) { return callee_dereference(nullptr); });

		printf("#cross call SEH dereference OK\n");
		benchAround([](int) { return callee_seh_dereference(&nextResult); });

		printf("#cross call SEH dereference nullptr\n");
		benchAround([](int) { return callee_seh_dereference(nullptr); });

		printf("#cross call UEH dereference OK\n");
		benchAround([](int) { return callee_ueh_dereference(&nextResult); });

		printf("#cross call UEH dereference nullptr\n");
		benchAround([](int) { return callee_ueh_dereference(nullptr); });

		printf("#stack-using cross-calls\n");
		printf("#board\tcaller stack\tcallee stack\tfull call\tcall\treturn\n");
		for (int callerUse = 128; callerUse <= 1024; callerUse <<= 1)
		{
			for (int calleeUse = 128; calleeUse <= 1024; calleeUse <<= 1)
			{
				for (int i = 0; i < 5; i++)
				{
					use_stack(callerUse);

					auto start  = METRIC();
					auto middle = callee_stack_using_return_metric(calleeUse);
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
		benchWithMiddle([](int) { return local_noop_return_metric(); });

		printf("#function call pointer manipulation\n");
		benchWithMiddle(
		  [](int) { return local_attenuate_return_metric(&nextResult); });

		printf("#library call\n");
		benchWithMiddle([](int) { return lib_noop_return_metric(); });

		printf("#dynamic token allocation\n");
		SKey tokens[5];
		benchAround([&](int i) { tokens[i] = token_key_new(); });

		printf("#token_sealed_unsealed_alloc\n");
		CHERI_SEALED(void *) sealeds[5];
		uint32_t *unsealeds[5];
		benchAround([&](int i) {
			Timeout timeout{UnlimitedTimeout};
			sealeds[i] = token_sealed_unsealed_alloc(
			  &timeout,
			  MALLOC_CAPABILITY,
			  tokens[0],
			  sizeof(uint32_t),
			  reinterpret_cast<void **>(&unsealeds[i]));
		});

		printf("#token_obj_unseal_dynamic\n");
		benchAround([&](int) {
			void *res = token_obj_unseal_dynamic(tokens[0], sealeds[0]);
			Debug::Assert(res == &unsealeds[0], "Bad unseal?");
		});
	}

	return 0;
}

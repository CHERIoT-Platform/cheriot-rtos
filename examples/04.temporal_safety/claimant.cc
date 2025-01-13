#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <fail-simulator-on-error.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Claimant compartment">;

void *x;

int __cheri_compartment("claimant") make_claim(void *ptr)
{
	Debug::log("Initial quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));

	if (x != nullptr)
	{
		free(x);
	}

	Timeout t{10};
	Debug::Invariant(heap_claim(MALLOC_CAPABILITY, ptr) != -ECOMPARTMENTFAIL,
	                 "Compartment call to heap_claim failed");
	x = ptr;

	Debug::log("Make Claim : {}", x);
	Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));
	return 0;
};

int __cheri_compartment("claimant") show_claim()
{
	Debug::log("Show Claim : {}", x);

	return 0;
}
// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>

#include "claimant.h"

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Allocating compartment">;

/// Thread entry point.
int __cheri_compartment("allocate") entry()
{
	// Simple case
	{
		Debug::log("----- Simple Case -----");
		void *x = malloc(42);
		// Print the allocated value:
		Debug::log("Allocated: {}", x);
		free(x);
		// Print the dangling pointer, note that it is no longer a valid pointer
		// (v:0)
		Debug::log("Use after free: {}", x);
	}

	// Sub object
	{
		Debug::log("----- Sub object -----");
		void *x = malloc(100);

		CHERI::Capability y{x};
		y.address() += 25;
		y.bounds() = 50;
		Debug::log("Allocated : {}", x);
		Debug::log("Sub Object: {}", y);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));

		// Free y - as it's a sub object of x both x & y remain valid
		free(y);
		Debug::log("After free of sub object");
		Debug::log("Allocated : {}", x);
		Debug::log("Sub Object: {}", y);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));

		// Free x - both x & y become invalid
		free(x);
		Debug::log("After free of allocation");
		Debug::log("Allocated : {}", x);
		Debug::log("Sub Object: {}", y);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));
	}

	// Sub object with a claim
	{
		Debug::log("----- Sub object with a claim -----");
		void *x = malloc(100);

		CHERI::Capability y{x};
		y.address() += 25;
		y.bounds() = 50;
		Debug::log("Allocated : {}", x);
		Debug::log("Sub Object: {}", y);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));

		// Add a claim for y - the quota remaining is reduced
		Debug::Invariant(heap_claim(MALLOC_CAPABILITY, y) > 0,
		                 "Compartment call to heap_claim failed");

		Debug::log("heap quota after claim: {}",
		           heap_quota_remaining(MALLOC_CAPABILITY));

		// free x.  As we have a claim on y both x and y remain valid
		free(x);
		Debug::log("After free of allocation");
		Debug::log("Allocated : {}", x);
		Debug::log("Sub Object: {}", y);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));

		// free y - releases the claim and both x & y become invalid
		free(y);
		Debug::log("After free of sub object");
		Debug::log("Allocated : {}", x);
		Debug::log("Sub Object: {}", y);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));
	}

	// Sub object with an ephemeral claim
	{
		Debug::log("----- Sub object with an ephemeral claim -----");
		void *x = malloc(100);

		CHERI::Capability y{x};
		y.address() += 25;
		y.bounds() = 50;
		Debug::log("Allocated : {}", x);
		Debug::log("Sub Object: {}", y);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));

		// Add an ephemeral claim for y
		Timeout t{10};
		heap_claim_ephemeral(&t, y);

		// In this freeing x will invalidate both x & y because free
		// is a cross compartment call, which releases any ephemeral claims.
		free(x);
		Debug::log("After free");
		Debug::log("Allocated : {}", x);
		Debug::log("Sub Object: {}", y);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));
	}

	// Using a claim in another compartment.
	// Note that a claim in the same compartment would also work, but this
	// shows the more typical use case
	{
		Debug::log("----- Claim in another compartment -----");
		void *x = malloc(10);

		Debug::log("Allocated : {}", x);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));

		// Get the claimant compartment to make a ephemeral claim
		Debug::Invariant(make_claim(x) == 0,
		                 "Compartment call to make_claim failed");

		// free x.  We get out quota back but x remains valid as
		// the claimant compartment has a claim on it
		free(x);
		Debug::log("After free: {}", x);
		Debug::log("heap quota: {}", heap_quota_remaining(MALLOC_CAPABILITY));

		// Get the claimant compartment to show its claim
		Debug::Invariant(show_claim() == 0,
		                 "Compartment call to show_claim failed");

		// Give the claimant another ptr so it releases the first
		void *y = malloc(10);
		Debug::Invariant(make_claim(y) == 0,
		                 "Compartment call to make_claim failed");
		Debug::log("After make claim");
		Debug::log("x: {}", x);
		Debug::log("y: {}", y);

		// Get the claimant compartment to show its new claim
		Debug::Invariant(show_claim() == 0,
		                 "Compartment call to show_claim failed");

		// tidy up
		free(y);
	}

	return 0;
}

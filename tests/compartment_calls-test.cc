// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment.h"
#define TEST_NAME "Compartment calls (main runner)"
#include "compartment_calls.h"
#include "tests.hh"
#include <atomic>
#include <cheri.hh>
#include <errno.h>
#include <switcher.h>
#include <thread_pool.h>

using namespace CHERI;

void test_number_of_arguments()
{
	debug_log(
	  "Test argument calls with different number and type of arguments");

	int ret;

	int value = ConstantValue;

	ret = compartment_call_inner(value);
	TEST(ret == 0, "compartment_call_inner returned {}", ret);

	ret = compartment_call_inner(value, value);
	TEST(ret == 0, "compartment_call_inner returned {}", ret);

	ret = compartment_call_inner(value, value, &value);
	TEST(ret == 0, "compartment_call_inner returned {}", ret);

	ret = compartment_call_inner(value, value, &value, value);
	TEST(ret == 0, "compartment_call_inner returned {}", ret);

	ret = compartment_call_inner(value, value, &value, value, &value);
	TEST(ret == 0, "compartment_call_inner returned {}", ret);

	ret = compartment_call_inner(value, value, &value, value, &value, value);
	TEST(ret == 0, "compartment_call_inner returned {}", ret);

	ret =
	  compartment_call_inner(value, value, &value, value, &value, value, value);
	TEST(ret == 0, "compartment_call_inner returned {}", ret);
}

__cheri_callback static int test_switcher_invocation_cpu_features_cb1()
{
	/*
	 * Verify that we see something nonzero on the way in and can clear it
	 * before returning to the caller
	 */
	auto vIn  = switcher_invocation_cpu_features_set(0, 0);
	auto vOut = switcher_invocation_cpu_features_set(-1, 0);
	return (vIn != vOut) && (vOut == 0);
}

/**
 * Test switcher_invocation_cpu_features_set()
 */
__noinline void test_switcher_invocation_cpu_features()
{
	// Turn everything off while reading the default
	uint32_t featuresDefault = switcher_invocation_cpu_features_set(0, 0);

	// Turn everything "on", whatever that means
	uint32_t featuresZero = switcher_invocation_cpu_features_set(0, -1);

	// Put back the default
	uint32_t featuresAll =
	  switcher_invocation_cpu_features_set(0, featuresDefault);

	// Read what should be the default again
	uint32_t featuresDefaultAgain = switcher_invocation_cpu_features_set(-1, 0);

	debug_log("switcher invocation CPU features: default={} zero={} all={}",
	          featuresDefault,
	          featuresZero,
	          featuresAll);

	TEST(featuresZero == 0, "Zero features nonzero?");
	TEST(featuresDefault == featuresDefaultAgain,
	     "Default features not restored");

	if (featuresAll != 0)
	{
		TEST((featuresAll & featuresDefault) == featuresDefault,
		     "switcher invocation CPU features: all & default != default?");

		switcher_invocation_cpu_features_set(0, 0);

		// Check that our callback fails if we call it with zero features on
		auto xZ = test_switcher_cpu_features_inner(
		  test_switcher_invocation_cpu_features_cb1);
		TEST(!xZ, "switcher invocation CPU features: bad inner call w/ 0");
		TEST(switcher_invocation_cpu_features_set(-1, 0) == featuresZero,
		     "switcher invocation CPU features: bad state after call w/ 0");

		switcher_invocation_cpu_features_set(0, featuresAll);

		// Check that our callback succeeds if we call it with all features on
		auto xA = test_switcher_cpu_features_inner(
		  test_switcher_invocation_cpu_features_cb1);
		TEST(xA, "switcher invocation CPU features: bad inner call w/ all");
		TEST(switcher_invocation_cpu_features_set(-1, 0) == featuresAll,
		     "switcher invocation CPU features: bad state after call w/ all");

		/*
		 * Check that a cross-thread switch picks up defaults for that thread,
		 * (rather than our "all" value, if that's different) and comes back to
		 * us with our "all" still OK.
		 */

		static std::atomic<uint32_t> state  = 0;
		int                          sleeps = 0;

		thread_pool::async([=]() {
			auto v0 = switcher_invocation_cpu_features_set(-1, 0);
			state   = (v0 == featuresDefault) ? 1 : 2;

			while (state.load() != 3)
			{
				// The main thread should have higher priority...
				yield();
			}

			switcher_invocation_cpu_features_set(0, 0);

			state = 4;
			while (state.load() != 5)
			{
				yield();
			}

			switcher_invocation_cpu_features_set(0, v0);
		});

		// Wait for the background worker to report in...
		while (state.load() == 0)
		{
			TEST(sleep(1) >= 0, "Failed to sleep");
			TEST(sleeps++ < 100, "Background thread failed to respond");
		}

		TEST(state.load() == 1, "Background thread saw bad CPU features");
		TEST(switcher_invocation_cpu_features_set(-1, 0) == featuresAll,
		     "switcher invocation CPU features: bad after 1st thread switch");

		// Wait for the background worker to change its CPU features
		state  = 3;
		sleeps = 0;
		while (state.load() != 4)
		{
			TEST(sleep(1) >= 0, "Failed to sleep");
			TEST(sleeps++ < 100, "Background thread failed to respond");
		}

		// Release the background thread (well, when we switch to it next)
		state = 5;

		TEST(switcher_invocation_cpu_features_set(-1, 0) == featuresAll,
		     "switcher invocation CPU features: bad after 2nd thread switch");
	}
	else
	{
		TEST(featuresDefault == 0,
		     "switcher invocation CPU features: all zero but nonzero default?");
		debug_log("switcher invocation CPU features: skip tests since all=0");
	}

	// Restore the default before continuing
	switcher_invocation_cpu_features_set(0, featuresDefault);
}

int test_compartment_calls()
{
	bool outTestFailed = false;
	int  ret           = 0;

	TEST(trusted_stack_has_space(0),
	     "Trusted stack should have space for 0 more calls");
	TEST(trusted_stack_has_space(1),
	     "Trusted stack should have space for 1 more calls");
	TEST(trusted_stack_has_space(7),
	     "Trusted stack should have space for 7 more calls");
	TEST(!trusted_stack_has_space(9),
	     "Trusted stack should not have space for 9 more calls");

	CHERI::Capability<void> csp{__builtin_cheri_stack_get()};
	CHERI::Capability<void> originalCSP{switcher_recover_stack()};
	csp.address() = originalCSP.address();
	TEST(csp == originalCSP,
	     "Original stack pointer: {}\ndoes not match current stack pointer: {}",
	     originalCSP,
	     csp);

	test_number_of_arguments();
	test_switcher_invocation_cpu_features();

	TEST_EQUAL(
	  test_incorrect_export_table(nullptr, &outTestFailed),
	  0,
	  "Test incorrect entry point without error handler bad return value");
	TEST(outTestFailed == false,
	     "Test incorrect entry point without error handler failed");
	return 0;
}

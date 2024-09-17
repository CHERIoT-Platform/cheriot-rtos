// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment.h"
#define TEST_NAME "Compartment calls (main runner)"
#include "compartment_calls.h"
#include "tests.hh"
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
	TEST(ret == 0, "compartment_call_inner returend {}", ret);

	ret = compartment_call_inner(value, value);
	TEST(ret == 0, "compartment_call_inner returend {}", ret);

	ret = compartment_call_inner(value, value, &value);
	TEST(ret == 0, "compartment_call_inner returend {}", ret);

	ret = compartment_call_inner(value, value, &value, value);
	TEST(ret == 0, "compartment_call_inner returend {}", ret);

	ret = compartment_call_inner(value, value, &value, value, &value);
	TEST(ret == 0, "compartment_call_inner returend {}", ret);

	ret = compartment_call_inner(value, value, &value, value, &value, value);
	TEST(ret == 0, "compartment_call_inner returend {}", ret);

	ret =
	  compartment_call_inner(value, value, &value, value, &value, value, value);
	TEST(ret == 0, "compartment_call_inner returend {}", ret);
}

void test_compartment_call()
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
	ret = test_incorrect_export_table_with_handler(nullptr);
	TEST(ret == -1, "Test incorrect entry point with error handler failed");

	test_incorrect_export_table(nullptr, &outTestFailed);
	TEST(outTestFailed == false,
	     "Test incorrect entry point without error handler failed");
}

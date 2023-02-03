// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Compartment calls (main runner)"
#include "compartment_calls.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>

using namespace CHERI;

void test_compartment_call()
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

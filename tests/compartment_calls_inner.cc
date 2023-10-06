// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Compartment calls (inner compartment)"
#include "compartment_calls.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>
#include <tuple>

using namespace CHERI;

std::tuple expectedArguments = {ConstantValue,
                                ConstantValue,
                                ConstantValue,
                                ConstantValue,
                                ConstantValue,
                                ConstantValue,
                                ConstantValue};

template<typename T, size_t I = std::tuple_size_v<T> - 1>
__attribute__((always_inline)) void check_tuple(T &args)
{
	auto arg      = std::get<I>(args);
	auto expected = std::get<I>(expectedArguments);
	TEST(
	  arg == expected, "argument == {}, expected value == {}", arg, expected);
	if constexpr (I > 0)
	{
		check_tuple<T, I - 1>(args);
	}
}
template<typename... Args>
__attribute__((always_inline)) void verify_arguments(Args... args)
{
	auto a = std::forward_as_tuple(args...);
	check_tuple(a);
}

int compartment_call_inner(int x0)
{
	debug_log("One argument");
	verify_arguments(x0);
	return 0;
}

int compartment_call_inner(int x0, int x1)
{
	debug_log("Two arguments");
	verify_arguments(x0, x1);
	return 0;
}

int compartment_call_inner(int x0, int x1, const int *x2)
{
	debug_log("Three arguments");
	verify_arguments(x0, x1, *x2);
	return 0;
}

int compartment_call_inner(int x0, int x1, const int *x2, int x3)
{
	debug_log("Four arguments");
	verify_arguments(x0, x1, *x2, x3);
	return 0;
}

int compartment_call_inner(int x0, int x1, const int *x2, int x3, const int *x4)
{
	debug_log("Five arguments");
	verify_arguments(x0, x1, *x2, x3, *x4);
	return 0;
}

int compartment_call_inner(int        x0,
                           int        x1,
                           const int *x2,
                           int        x3,
                           const int *x4,
                           int        x5)
{
	debug_log("Six arguments");
	verify_arguments(x0, x1, *x2, x3, *x4, x5);
	return 0;
}

int compartment_call_inner(int        x0,
                           int        x1,
                           const int *x2,
                           int        x3,
                           const int *x4,
                           int        x5,
                           int        x6)
{
	debug_log("Seven arguments");
	verify_arguments(x0, x1, *x2, x3, *x4, x5, x6);
	return 0;
}

void test_incorrect_export_table(__cheri_callback void (*fn)(),
                                 bool *outTestFailed)
{
	/*
	 * Trigger a cross-compartment call with an invalid export entry.
	 */

	debug_log("test an incorrect export table entry");

	*outTestFailed = true;

	fn();

	*outTestFailed = false;
}

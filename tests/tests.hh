// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef CLANG_TIDY
#	include "tests-prototypes.h"
#else
// NOLINTNEXTLINE(readability-identifier-naming)
static const int TestEnabled_preflight = 1;
#endif
#include <cdefs.h>
#include <debug.hh>
#include <thread.h>

int print_version_information();

using Test = ConditionalDebug<true,
#ifdef TEST_NAME
                              TEST_NAME " test"
#else
                              "Test runner"
#endif
                              >;

template<typename... Args>
void debug_log(const char *fmt, Args... args)
{
	Test::log(fmt, std::forward<Args>(args)...);
}

#define TEST(cond, msg, ...) Test::Invariant((cond), msg, ##__VA_ARGS__)
#define TEST_EQUAL(e1, e2, msg)                                                \
	{                                                                          \
		auto v1 = e1;                                                          \
		auto v2 = e2;                                                          \
		Test::Invariant(v1 == v2, "{}: {} != {}", msg, v1, v2);                \
	}

/**
 * Macro intented to check for failure of function or compartment calls.
 * Evaluates e1 and asserts an invariant that it is greater than or equal to 0.
 * Prints the message and returned value on failure.
 */
#define TEST_SUCCESS(e1)                                                       \
	{                                                                          \
		auto v1 = e1;                                                          \
		Test::Invariant(v1 >= 0, "{} failed: {}", #e1, v1);                    \
	}

/**
 * Helper to sleep for a number of ticks and not report the sleep time.
 */
inline Ticks sleep(Ticks ticks)
{
	Timeout t{ticks};
	TEST_SUCCESS(thread_sleep(&t));
	return t.elapsed;
};

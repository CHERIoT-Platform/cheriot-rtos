// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Global Constructor"
#include "tests.hh"
#include <global_constructors.hh>

/// A class with a non-trivial constructor and a global instance of the class.
struct X
{
	/// A global incremented on construction.
	static inline int setInCtor;
	/// Non-trivial constructor.
	X()
	{
		debug_log("Running global constructor");
		setInCtor++;
	}
} x;

/// Check that we run global constructors precisely once.
void test_global_constructors()
{
	TEST(X::setInCtor == 0, "Constructor run surprisingly early");
	GlobalConstructors::run();
	TEST(X::setInCtor == 1, "Constructor not run");
	GlobalConstructors::run();
	TEST(X::setInCtor == 1, "Constructor run twice");
}

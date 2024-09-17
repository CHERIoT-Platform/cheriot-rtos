// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Test unwind cleanup"
#include "locks.hh"
#include "tests.hh"
#include "unwind.h"

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *, size_t, size_t)
{
	cleanup_unwind();
	return ErrorRecoveryBehaviour::ForceUnwind;
}

namespace
{
	void test_setjmp()
	{
		jmp_buf      env;
		volatile int x = 0;
		if (int r = setjmp(env); r == 0)
		{
			TEST_EQUAL(x, 0, "setjmp should return 0 the first time");
			x = 42;
			longjmp(env, 1);
		}
		else
		{
			TEST_EQUAL(r, 1, "setjmp should return 1 the second time");
			TEST_EQUAL(
			  x, 42, "On the second return, x should have been modified");
			x = 53;
		}
		TEST_EQUAL(x, 53, "After longjmp, x should have been modified");
	}

	FlagLock flagLock;

	void test_on_error()
	{
		LockGuard g(flagLock);
		on_error([&]() { cleanup_unwind(); }, [&]() { g.unlock(); });
		TEST(!g, "on_error should lock the lock");
	}

	void test_on_error_raii_inner()
	{
		LockGuard g(flagLock);
		// No handler.  g's destructor runs after on_error returns.
		on_error([&]() { cleanup_unwind(); });
	}

	void test_on_error_raii()
	{
		test_on_error_raii_inner();
		TEST(flagLock.try_lock(), "raii should have been dropped the lock");
		flagLock.unlock();
	}

	void test_c_macros()
	{
		volatile int x = 0;
		CHERIOT_DURING
		{
			x = 42;
			cleanup_unwind();
		}
		CHERIOT_HANDLER
		{
			TEST_EQUAL(x, 42, "In the handler, x should have been modified");
			x = 53;
		}
		CHERIOT_END_HANDLER
		TEST_EQUAL(x, 53, "After longjmp, object should have been modified");
	}

	__noinline void overflow_stack(volatile int *x)
	{
		int hugeBuffer[4096];
		debug_log("Overflowing stack: {}", hugeBuffer);
	}

	/**
	 * Make sure that we can unwind out of a trap.  This will invoke the normal
	 * error handler.
	 */
	void test_from_trap()
	{
		volatile int x = 0;
		CHERIOT_DURING
		{
			x = 42;
			__builtin_trap();
		}
		CHERIOT_HANDLER
		{
			TEST_EQUAL(x, 42, "In the handler, x should have been modified");
			x = 53;
		}
		CHERIOT_END_HANDLER
		TEST_EQUAL(
		  x, 53, "After error handler, object should have been modified");
	}

	/**
	 * Make sure that we can unwind out of a stack overflow.  This will invoke
	 * the stackless error handler.
	 */
	void test_from_stack_overflow()
	{
		volatile int x = 0;
		CHERIOT_DURING
		{
			x = 42;
			overflow_stack(&x);
		}
		CHERIOT_HANDLER
		{
			debug_log("Error handler");
			TEST_EQUAL(x, 42, "In the handler, x should have been modified");
			x = 53;
		}
		CHERIOT_END_HANDLER
		TEST_EQUAL(x, 53, "After handler, object should have been modified");
	}

} // namespace

void test_unwind_cleanup()
{
	test_setjmp();
	test_on_error();
	test_c_macros();
	// Try these in both orders to make sure that both error handlers correctly
	// clean up.
	test_from_trap();
	test_from_stack_overflow();

	test_from_stack_overflow();
	test_from_trap();
	debug_log("Test unwind_cleanup passed");
}

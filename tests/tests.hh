// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <debug.hh>
#include <thread.h>

__cheri_compartment("eventgroup_test") int test_eventgroup();
__cheri_compartment("mmio_test") int test_mmio();
__cheri_compartment("allocator_test") int test_allocator();
__cheri_compartment("thread_pool_test") int test_thread_pool();
__cheri_compartment("futex_test") int test_futex();
__cheri_compartment("queue_test") int test_queue();
__cheri_compartment("locks_test") int test_locks();
__cheri_compartment("list_test") int test_list();
__cheri_compartment("crash_recovery_test") int test_crash_recovery();
__cheri_compartment("multiwaiter_test") int test_multiwaiter();
__cheri_compartment("stack_test") int test_stack();
__cheri_compartment("compartment_calls_test") int test_compartment_call();
__cheri_compartment("check_pointer_test") int test_check_pointer();
__cheri_compartment("misc_test") int test_misc();
__cheri_compartment("static_sealing_test") int test_static_sealing();
__cheri_compartment("stdio_test") int test_stdio();
__cheri_compartment("debug_test") int test_debug_cxx();
__cheri_compartment("debug_test") int test_debug_c();
__cheri_compartment("unwind_cleanup_test") int test_unwind_cleanup();
__cheri_compartment("isa_test") int test_isa();

// Simple tests don't need a separate compartment.
void test_global_constructors();

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
	Test::log(fmt, args...);
}

#define TEST(cond, msg, ...) Test::Invariant((cond), msg, ##__VA_ARGS__)
#define TEST_EQUAL(e1, e2, msg)                                                \
	{                                                                          \
		auto v1 = e1;                                                          \
		auto v2 = e2;                                                          \
		Test::Invariant(v1 == v2, "{}: {} != {}", msg, v1, v2);                \
	}

/**
 * Helper to sleep for a number of ticks and not report the sleep time.
 */
inline Ticks sleep(Ticks ticks)
{
	Timeout t{ticks};
	thread_sleep(&t);
	return t.elapsed;
};

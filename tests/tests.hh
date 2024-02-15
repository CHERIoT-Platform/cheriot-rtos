// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <debug.hh>
#include <thread.h>

__cheri_compartment("eventgroup_test") void test_eventgroup();
__cheri_compartment("mmio_test") void test_mmio();
__cheri_compartment("allocator_test") void test_allocator();
__cheri_compartment("thread_pool_test") void test_thread_pool();
__cheri_compartment("futex_test") void test_futex();
__cheri_compartment("queue_test") void test_queue();
__cheri_compartment("locks_test") void test_locks();
__cheri_compartment("crash_recovery_test") void test_crash_recovery();
__cheri_compartment("multiwaiter_test") void test_multiwaiter();
__cheri_compartment("stack_test") void test_stack();
__cheri_compartment("compartment_calls_test") void test_compartment_call();
__cheri_compartment("check_pointer_test") void test_check_pointer();
__cheri_compartment("static_sealing_test") void test_static_sealing();

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

/**
 * Helper to sleep for a number of ticks and not report the sleep time.
 */
inline Ticks sleep(Ticks ticks)
{
	Timeout t{ticks};
	thread_sleep(&t);
	return t.elapsed;
};

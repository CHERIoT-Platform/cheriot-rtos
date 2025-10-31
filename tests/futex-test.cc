// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Futex"
#include "tests.hh"
#include <cheri.hh>
#include <cheriot-atomic.hh>
#include <errno.h>
#include <futex.h>
#include <interrupt.h>
#include <thread.h>
#include <thread_pool.h>

using namespace CHERI;
using namespace thread_pool;

#ifdef SAIL
DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(interruptCapability,
                                        FakeInterrupt,
                                        true,
                                        true);
#endif

int test_futex()
{
	static uint32_t futex;
	int             ret;
	int             sleeps;
	// Make sure that waking a futex with no sleepers doesn't crash!
	ret = futex_wake(&futex, 1);
	TEST(ret == 0, "Waking a futex with no sleepers should return 0");
	// Wake in another thread, ensure that we don't wake until the futex value
	// has been set to 1.
	async([]() {
		futex = 1;
		(void)futex_wake(&futex, 1);
	});
	debug_log("Calling blocking futex_wait");
	ret = futex_wait(&futex, 0);
	TEST(ret == 0, "futex_wait returned {}, expected 0", ret);
	TEST(futex == 1,
	     "Thread should not have woken up until the futex had been set to 1");
	// Check that a futex wait with a mismatched value returns immediately.
	debug_log("Calling futex_wait and expecting immediate return");
	ret = futex_wait(&futex, 0);
	TEST(ret == 0, "futex_wait returned {}, expected 0", ret);
	debug_log("Calling futex with timeout");
	{
		Timeout t{3};
		auto    err = futex_timed_wait(&t, &futex, 1);
		TEST(err == -ETIMEDOUT,
		     "futex_timed_wait returned {}, expected {}",
		     err,
		     -ETIMEDOUT);
		TEST(t.elapsed >= 3,
		     "futex_timed_wait timed out but elapsed ticks {} too small",
		     t.elapsed);
	}
	Timeout t{3};
	auto    err = futex_timed_wait(&t, &futex, 0);
	TEST(err == 0, "futex_timed_wait returned {}, expected {}", err, 0);
	// Some tests for invalid API usage.
	ret = futex_wait(nullptr, 0);
	TEST(ret == -EINVAL,
	     "futex_wait returned {} when called with a null pointer, expected {}",
	     ret,
	     -EINVAL);
	ret = futex_wake(nullptr, 0);
	TEST(ret == -EINVAL,
	     "futex_wake returned {} when called with a null pointer, expected {}",
	     ret,
	     -EINVAL);
	Capability fcap{&futex};
	fcap.permissions() &= PermissionSet::omnipotent().without(Permission::Load);
	ret = futex_wait(fcap, 0);
	TEST(
	  ret == -EINVAL,
	  "futex_wait returned {} when called without load permission, expected {}",
	  ret,
	  -EINVAL);
	fcap = &futex;
	fcap.permissions() &=
	  PermissionSet::omnipotent().without(Permission::Store);
	ret = futex_wake(fcap, 1);
	TEST_EQUAL(
	  ret, 0, "futex_wake returned when called without store permission");

#ifdef SAIL
	// If we're targeting Sail, also do some basic tests of the interrupt
	// capability.  We don't have an interrupt controller, so these tests are
	// quite rudimentary.
	Capability<const uint32_t> interruptFutex =
	  interrupt_futex_get(STATIC_SEALED_VALUE(interruptCapability));
	TEST(interruptFutex.is_valid(),
	     "Interrupt futex {} is not valid",
	     interruptFutex);
	TEST(!interruptFutex.permissions().contains(Permission::Store),
	     "Interrupt futex {} should not have store permission",
	     interruptFutex);
	TEST(*interruptFutex == 0,
	     "Interrupt futex {} for fake interrupt should not have fired but "
	     "shows {} interrupts",
	     interruptFutex,
	     *interruptFutex);
	TEST(interrupt_complete(STATIC_SEALED_VALUE(interruptCapability)) == 0,
	     "interrupt_complete returned an error unexpectedly");
	TEST(interrupt_complete(nullptr) != 0,
	     "interrupt_complete returned success unexpectedly");
#endif

	debug_log("Starting priority inheritance test");
	futex = 0;

	static cheriot::atomic<int> state;
	auto                        priorityBug = []() {
        if (thread_id_get() == 2)
        {
            debug_log("Medium-priority task starting");
            // We are the high priority thread, wait for the futex to be
            // acquired and then spin and never yield.
            while (state != 1)
            {
                Timeout t{3};
                TEST_SUCCESS(thread_sleep(&t));
            }
            debug_log("Consuming all CPU on medium-priority thread");
            state = 2;
            while (state != 4) {}
            debug_log("Finishing medium-priority task");
        }
        else
        {
            debug_log("Low-priority task starting");
            futex = thread_id_get();
            state = 1;
            debug_log("Low-priority thread acquired futex, yielding");
            // Now the high priority thread should run.
            while (state != 3)
            {
                yield();
            }
            debug_log("Low-priority thread finished, unlocking");
            state = 4;
            futex = 0;
            TEST_SUCCESS(futex_wake(&futex, 1));
        }
	};
	async(priorityBug);
	async(priorityBug);
	debug_log("Waiting for background threads to enter the right state");
	for (sleeps = 0; (sleeps < 100) && (state != 2); sleeps++)
	{
		TEST(sleep(3) >= 0, "Failed to sleep");
	}
	TEST(sleeps < 100, "Waited too long for background threads");
	debug_log("High-priority thread attempting to acquire futex owned by "
	          "low-priority thread without priority propagation");
	state       = 3;
	t.remaining = 1;
	TEST_EQUAL(futex_timed_wait(&t, &futex, futex, FutexNone),
	           -ETIMEDOUT,
	           "futex_timed_wait failed");
	TEST(futex != 0, "Made progress surprisingly!");
	debug_log("High-priority thread attempting to acquire futex owned by "
	          "low-priority thread with priority propagation");
	t.remaining = 4;
	TEST_EQUAL(futex_timed_wait(&t, &futex, futex, FutexPriorityInheritance),
	           0,
	           "futex_timed_wait failed");
	TEST(futex == 0, "Failed to make progress!");

	futex       = 1234;
	t.remaining = 1;
	ret         = futex_timed_wait(&t, &futex, futex, FutexPriorityInheritance);
	TEST(ret == -EINVAL,
	     "PI futex with an invalid thread ID returned {}, should be {}",
	     ret,
	     -EINVAL);

	futex = 0;
	debug_log(
	  "Testing priority inheriting futex_timed_wait with zero thread ID");
	// zero is not a valid thread ID for priority inheriting futex. Previously
	// this caused a crash due to OOB access but better to return -EINVAL.
	ret = futex_timed_wait(&t, &futex, 0, FutexPriorityInheritance);
	TEST(ret == -EINVAL,
	     "PI futex with a zero thread ID returned {}, should be {}",
	     ret,
	     -EINVAL);
	return 0;
}

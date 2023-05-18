// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Futex"
#include "tests.hh"
#include <cheri.hh>
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

void test_futex()
{
	static uint32_t futex;
	int             ret;
	// Make sure that waking a futex with no sleepers doesn't crash!
	ret = futex_wake(&futex, 1);
	TEST(ret == 0, "Waking a futex with no sleepers should return 0");
	// Wake in another thread, ensure that we don't wake until the futex value
	// has been set to 1.
	async([]() {
		futex = 1;
		futex_wake(&futex, 1);
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
	TEST(ret == -EINVAL,
	     "futex_wake returned {} when called without store permission, "
	     "expected {}",
	     ret,
	     -EINVAL);

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
}

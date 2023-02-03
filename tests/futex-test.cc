// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Futex"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>
#include <futex.h>
#include <thread.h>
#include <thread_pool.h>

using namespace CHERI;
using namespace thread_pool;

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
		auto    err = futex_timed_wait(&futex, 1, &t);
		TEST(err == -ETIMEDOUT,
		     "futex_timed_wait returned {}, expected {}",
		     err,
		     -ETIMEDOUT);
		TEST(t.elapsed >= 3,
		     "futex_timed_wait timed out but elapsed ticks {} too small",
		     t.elapsed);
	}
	Timeout t{3};
	auto    err = futex_timed_wait(&futex, 0, &t);
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
}

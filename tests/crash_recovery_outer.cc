// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Crash recovery (outer compartment)"
#include "crash_recovery.h"
#include <cheri.hh>
#include <errno.h>

void test_crash_recovery_outer(int)
{
	debug_log(
	  "Calling crashy compartment from compartment with no error handler");
	CHERI::Capability<void> ret = test_crash_recovery_inner(0);
	check_stack();
	TEST(ret.is_valid() == false, "Error returned a tagged capability {}", ret);
	TEST(ret.address() == -1,
	     "Error return path returned number that is not -1: {}",
	     ret.address());
	debug_log("Calling crashy compartment returned to compartment with no "
	          "error handler.  Return value: {}",
	          ret);
}

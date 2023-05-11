// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Crash recovery (main runner)"
#include "crash_recovery.h"
#include <cheri.hh>
#include <errno.h>

int crashes = 0;

extern "C" enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("Test saw error for PCC {}", frame->pcc);
	debug_log("Error cause: {}, mtval: {}", mcause, mtval);
	if (mcause == 0x2)
	{
		debug_log("Test hit assertion failure, unwinding");
		return ErrorRecoveryBehaviour::ForceUnwind;
	}
	TEST((mcause == 0x1c) && (mtval == 0),
	     "mcause should be 0x1c (CHERI), is {}, mtval should be 0 (force "
	     "unwind), is {})",
	     mcause,
	     mtval);
	crashes++;
	debug_log("Resuming test at failure location");
	return ErrorRecoveryBehaviour::InstallContext;
}

void test_crash_recovery()
{
	debug_log("Calling crashy compartment indirectly");
	test_crash_recovery_outer(0);
	check_stack();
	TEST(crashes == 0, "Ran crash handler for outer compartment");
	debug_log("Compartment with no error handler returned normally after "
	          "nested call crashed");

	debug_log("Calling crashy compartment to fault and unwind");
	test_crash_recovery_inner(0);
	check_stack();
	debug_log("Calling crashy compartment returned (crashes: {})", crashes);
	TEST(crashes == 1, "Failed to notice crash");

	debug_log("Calling crashy compartment to return normally");
	test_crash_recovery_inner(1);
	check_stack();
	debug_log("Calling crashy compartment returned (crashes: {})", crashes);
	TEST(crashes == 1, "Should not have crashed");
	debug_log("Returning normally from crash test");

	debug_log("Calling crashy compartment to double fault and unwind");
	test_crash_recovery_inner(2);
	check_stack();
	debug_log("Calling crashy compartment returned (crashes: {})", crashes);
	TEST(crashes == 2, "Failed to notice crash");
}

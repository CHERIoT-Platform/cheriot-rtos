// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Crash recovery (main runner)"
#include "crash_recovery.h"
#include <atomic>
#include <cheri.hh>
#include <errno.h>

int               crashes = 0;
std::atomic<bool> expectFault;

extern "C" enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame, size_t mcause, size_t mtval)
{
	crashes++;
	if (mcause == 0x2)
	{
		if (expectFault)
		{
			expectFault = false;
			frame->pcc  = static_cast<char *>(frame->pcc) + 2;
			return ErrorRecoveryBehaviour::InstallContext;
		}
		debug_log("Test hit assertion failure, unwinding");
		return ErrorRecoveryBehaviour::ForceUnwind;
	}
	debug_log("Test saw error for PCC {}", frame->pcc);
	debug_log("Error cause: {}, mtval: {}", mcause, mtval);
	TEST((mcause == 0x1c) && (mtval == 0),
	     "mcause should be 0x1c (CHERI), is {}, mtval should be 0 (force "
	     "unwind), is {})",
	     mcause,
	     mtval);
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

	debug_log(
	  "Calling crashy compartment to corrupt CSP in stack pointer and unwind");
	test_crash_recovery_inner(3);
	check_stack();
	debug_log("Calling crashy compartment returned (crashes: {})", crashes);
	TEST(crashes == 3, "Failed to notice crash");

	ptraddr_t handlerCount = switcher_handler_invocation_count_reset();
	TEST(handlerCount == crashes * 2,
	     "Should have called handler 3 times (3 entries, 3 exits giving a "
	     "total of {}), was {}",
	     crashes * 2,
	     handlerCount);

	// By default, we will be force unwound if we exceed 512 error-handler
	// invocations.
	constexpr int MaxCrashes = 600;
	for (int i = 0; i < MaxCrashes; i++)
	{
		switcher_handler_invocation_count_reset();
		expectFault = true;
		// Crash with a guaranteed 16-bit instruction.  This cannot use
		// `__builtin_trap` because the compiler knows that `__builtin_trap`
		// does not return and so will not generate code following it.
		asm volatile("c.unimp");
	}
	TEST(crashes == 3 + MaxCrashes, "Failed to notice crash");
}

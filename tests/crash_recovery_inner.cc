// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Crash recovery (inner compartment)"
#include "crash_recovery.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>
#include <new>

using namespace CHERI;

volatile bool                   shouldDoubleFault             = false;
volatile bool                   shouldSkipFaultingInstruction = false;
volatile ErrorRecoveryBehaviour recoveryBehaviour;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("Detected error in instruction {}", frame->pcc);
	debug_log("Error cause: {}", mcause);
	if (shouldSkipFaultingInstruction)
	{
		Capability pcc{__builtin_cheri_program_counter_get()};
		pcc.address() = Capability{frame->pcc}.address();
		uint32_t faultingInstruction;
		// pcc may be unaligned, so we need a memcpy to load from it.
		memcpy(&faultingInstruction, pcc, 4);
		debug_log("Faulting instruction: {}", faultingInstruction);
		// If the low bits are 11 then this is a 32-bit instruction, otherwise
		// it's a 16-bit one.
		ptrdiff_t skipSize = ((faultingInstruction & 3) == 3) ? 4 : 2;
		frame->pcc         = static_cast<char *>(frame->pcc) + skipSize;
	}
	if (shouldDoubleFault)
	{
		debug_log("Triggering double fault");
		// Should trigger a permit-store violation
		auto readOnlyPointer =
		  static_cast<char *>(__builtin_cheri_program_counter_get());
		readOnlyPointer[0] = 1;
	}
	return recoveryBehaviour;
}

void *test_crash_recovery_inner(int option)
{
	int  x[16];
	int *ptr      = std::launder(x);
	auto capFault = [=]() {
		__c11_atomic_signal_fence(__ATOMIC_SEQ_CST);
		ptr[16] = 0;
		__c11_atomic_signal_fence(__ATOMIC_SEQ_CST);
	};
	switch (option)
	{
		case 0:
			// simple crash
			shouldDoubleFault             = false;
			shouldSkipFaultingInstruction = false;
			debug_log("Trying to store out of bounds in {}, simple unwind",
			          ptr);
			recoveryBehaviour = ErrorRecoveryBehaviour::ForceUnwind;
			capFault();
			TEST(false, "Should be unreachable");
		case 1:
			// Skip, return normally
			shouldDoubleFault             = false;
			shouldSkipFaultingInstruction = true;
			recoveryBehaviour = ErrorRecoveryBehaviour::InstallContext;
			capFault();
			debug_log("Store silently ignored");
			return nullptr;
		case 2:
			// Double fault
			shouldDoubleFault             = true;
			shouldSkipFaultingInstruction = true;
			recoveryBehaviour = ErrorRecoveryBehaviour::InstallContext;
			debug_log("Trying to fault and double fault in the error handler");
			capFault();
			TEST(false, "Double fault resumed");
	}
	return nullptr;
}

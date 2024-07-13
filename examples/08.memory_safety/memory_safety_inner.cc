// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Memory safety (inner compartment)"
#include "memory_safety.h"
#include <cheri.hh>
#include <debug.hh>
#include <errno.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Memory safety compartment">;

using namespace CHERI;

char *allocation;
static char *volatile volatilePointer;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	auto [exceptionCode, registerNumber] = extract_cheri_mtval(mtval);
	void **faultingRegister = frame->get_register_value(registerNumber);
	Debug::Invariant(faultingRegister != nullptr,
	                 "get_register_value returned NULL unexpectedly");
	Debug::log("Detected error in instruction {}", frame->pcc);
	Debug::log("Detected {}: Register {} contained "
	           "invalid value: {}",
	           exceptionCode,
	           registerNumber,
	           *faultingRegister);

	/*
	 * `free` already checks for non-NULL and capability's validity
	 */
	free(allocation);

	return ErrorRecoveryBehaviour::ForceUnwind;
}

int memory_safety_inner_entry(MemorySafetyBugClass operation)
{
	size_t length = 0x100;
	switch (operation)
	{
		case MemorySafetyBugClass::StackLinearOverflow:
		{
			/*
			 * Trigger a stack linear overflow overflow, by storing one byte
			 * beyond an allocation bounds. The bounds checks are performed in
			 * the architectural level, by the CPU. Each capability carries the
			 * allocation bounds.
			 */
			int  arr[0x10];
			int *ptr      = std::launder(arr);
			auto capFault = [=]() {
				__c11_atomic_signal_fence(__ATOMIC_SEQ_CST);
				ptr[sizeof(arr) / sizeof(arr[0])] = 0;
				__c11_atomic_signal_fence(__ATOMIC_SEQ_CST);
				Debug::log("use {}", ptr[sizeof(arr) / sizeof(arr[0])]);
			};

			Debug::log("Trigger stack linear overflow");
			capFault();

			Debug::Assert(false, "Code after overflow should be unreachable");
		}
		case MemorySafetyBugClass::HeapLinearOverflow:
		{
			/*
			 * Trigger a linear overflow on the heap, by storing one byte beyond
			 * an allocation bounds. The bounds checks are performed in the
			 * architectural level, by the CPU. Each capability carries the
			 * allocation bounds.
			 */
			allocation = static_cast<char *>(malloc(length));
			Debug::Assert(allocation != NULL,
			              "Allocation failed in HeapLinearOverflow");

			Debug::log("Trigger heap linear overflow");
			allocation[length] = '\x41';

			Debug::Assert(false, "Code after overflow should be unreachable");
		}
		case MemorySafetyBugClass::HeapNonlinearOverflow:
		{
			/*
			 * Trigger a non-linear overflow on the heap, by triggering a store
			 * beyond the allocation bounds. The bounds checks are performed in
			 * the architectural level, by the CPU. Each capability carries the
			 * allocation bounds.
			 */
			allocation = static_cast<char *>(malloc(length));
			Debug::Assert(allocation != NULL,
			              "Allcoation failed in HeapNonlinearOverflow");

			Debug::log("Trigger heap nonlinear overflow");
			allocation[length * 2] = '\x41';

			Debug::Assert(false, "Code after overflow should be unreachable");
		}
		case MemorySafetyBugClass::HeapUseAfterFree:
		{
			/*
			 * Trigger a use after free, by storing a byte to an allocation
			 * beyond the end of its lifetime.
			 */
			allocation = static_cast<char *>(malloc(length));
			Debug::Assert(allocation != NULL,
			              "Allcoation failed in HeapUseAfterFree");

			free(allocation);

			/*
			 * From this point forward, any dereference of any dangling pointer
			 * to the freed memory will trap. This is guaranteed by the hardware
			 * load barrier that, on loads of capabilities to the memory region
			 * that can be used as a heap, checks the revocation bit
			 * corresponding to the base of the capability and clears the tag if
			 * it is set. For more details, see docs/architecture.md.
			 */
			Debug::log("Trigger heap use after free");
			allocation[0] = '\x41';

			Debug::Assert(false,
			              "Code after use after free should be unreachable");
		}
		case MemorySafetyBugClass::StoreStackPointerToGlobal:
		{
			/*
			 * Storing a stack pointer to a global variable makes it invalid.
			 * This is enforced by the Global (G) permission bit in the
			 * capability.
			 * This provides strong thread-isolation guarantees: data stored
			 * on the stack is never vulnerable to concurrent mutation.
			 */

			char buf[0x10];
			Debug::log("Trigger storing a stack pointer {} into global",
			           Capability{buf});
			volatilePointer = buf;
			Capability tmp  = volatilePointer;
			Debug::log("tmp: {}", tmp);
			Debug::Assert(!tmp.is_valid(),
			              "Stack pointer stored into global should be invalid");
			return tmp[0];
		}
	}

	return 0;
}

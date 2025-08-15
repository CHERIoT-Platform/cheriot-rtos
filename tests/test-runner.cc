// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "tests.hh"
#include <compartment.h>
#include <simulator.h>
#include <string>

using namespace CHERI;
using namespace std::string_literals;

namespace
{
	/// Have we detected a crash in any of the compartments?
	volatile bool crashDetected = false;

	/**
	 * Read the cycle counter.
	 */
	int rdcycle()
	{
		int cycles;
#ifdef SAIL
		// On Sail, report the number of instructions, the cycle count is
		// meaningless.
		__asm__ volatile("csrr %0, minstret" : "=r"(cycles));
#elif defined(IBEX)
		__asm__ volatile("csrr %0, mcycle" : "=r"(cycles));
#else
		__asm__ volatile("rdcycle %0" : "=r"(cycles));
#endif
		return cycles;
	}
	/**
	 * Call `fn` and log a message informing the user how long it took.
	 */
	void run_timed(const char *msg, auto &&fn)
	{
		bool failed      = false;
		int  startCycles = rdcycle();
		if constexpr (std::is_same_v<std::invoke_result_t<decltype(fn)>, void>)
		{
			fn();
		}
		else
		{
			failed = (fn() != 0);
		}
		int cycles = rdcycle();

		if (failed)
		{
			debug_log("{} failed", msg);
			crashDetected = true;
		}
		else
		{
			debug_log("{} finished in {} cycles", msg, cycles - startCycles);
		}
	}
} // namespace

extern "C" enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("mcause: {}, pcc: {}, cra: {}",
	          mcause,
	          frame->pcc,
	          frame->get_register_value<RegisterNumber::CRA>());
	auto [reg, cause] = CHERI::extract_cheri_mtval(mtval);
	debug_log("Error {} in register {}", reg, cause);
	if (mcause == 0x2)
	{
		debug_log("Test failure in test runner");
		simulation_exit(1);
	}
	return ErrorRecoveryBehaviour::InstallContext;
}

__attribute__((noinline, weak)) void *pcc_as_sentry()
{
	return __builtin_return_address(0);
}

/**
 * Test suite entry point.  Runs all of the tests that we have defined.
 */
[[cheriot::interrupt_state(disabled)]]
int __cheri_compartment("test_runner") run_tests()
{
	print_version_information();

	if constexpr (TestEnabled_preflight)
	{
		{
			// Inspect the return sentry the switcher gave us.
			// Since the switcher does not need Global on PCC it runs with a
			// local one, meaning the return sentry we receive should also be
			// local, which is fine as we have no reason to store it anywhere
			// except the stack.
			Capability switcherReturnSentry{__builtin_return_address(0)};
			TEST(
			  !switcherReturnSentry.permissions().contains(Permission::Global),
			  "Switcher return sentry should be local");
		}

		{
			// The 0th import table entry is a sentry to the switcher's
			// compartment_switcher_entry().  Despite being in global memory, it
			// should be a local capability.
			void *switcherCrossCallRaw;
			__asm__ volatile(
			  "1:\n"
			  "auipcc %0, %%cheriot_compartment_hi(.compartment_switcher)\n"
			  "clc %0, %%cheriot_compartment_lo_i(1b)(%0)\n"
			  : "=C"(switcherCrossCallRaw));
			Capability switcherCrossCall{switcherCrossCallRaw};
			TEST(!switcherCrossCall.permissions().contains(Permission::Global),
			     "Switcher cross-call sentry should be local");
			TEST(switcherCrossCall.type() == CheriSealTypeSentryDisabling,
			     "Switcher cross-call sentry should be IRQ-disabling forward "
			     "sentry");
		}

		// This is started as an interrupts-disabled thread, make sure that it
		// really is!  This should always be CheriSealTypeReturnSentryDisabling,
		// but older ISA versions didn't have separate forward and backwards
		// sentries and so allow any kind of interrupt-disabled sentry.
		Capability sealedPCC = pcc_as_sentry();
		TEST((sealedPCC.type() == CheriSealTypeReturnSentryDisabling) ||
		       (sealedPCC.type() == CheriSealTypeSentryDisabling),
		     "Entry point does not run with interrupts disabled: {}",
		     sealedPCC);

		// magic_enum is a pretty powerful stress-test of various bits of
		// linkage. In generating `enum_values`, it generates constant strings
		// and pointers to them in COMDAT sections.  These require merging
		// across compilation units during our first link stage and then require
		// cap relocs so that they are all correctly bounded.  This works as a
		// good smoke test of our linker script.
		debug_log(
		  "Checking that rel-ro caprelocs work.  This will crash if they "
		  "don't.  CHERI Permissions are:");
		for (auto &permission : magic_enum::enum_values<CHERI::Permission>())
		{
			debug_log("{}", permission);
		}

		/*
		 * Add a test to check the PermissionSet Iterator. We swap the
		 * permissions order to match it to the order the Iterator implements:
		 * the Iterator returns then in value order, and not insertion order.
		 */
		static constexpr PermissionSet Permissions{
		  Permission::Load, Permission::Store, Permission::LoadStoreCapability};
		static Permission permissionArray[] = {
		  Permission::Store, Permission::Load, Permission::LoadStoreCapability};
		size_t index = 0;
		for (auto permission : Permissions)
		{
			TEST_EQUAL(permission,
			           permissionArray[index++],
			           "Iterator of PermissionSet failed");
		}
		// These need to be checked visually
		debug_log("Trying to print 8-bit integer: {}",
		          static_cast<uint8_t>(0x12));
		debug_log("Trying to print unsigned 8-bit integer: {}",
		          static_cast<int8_t>(34));
		debug_log("Trying to print char: {}", 'c');
		debug_log("Trying to print 32-bit integer: {}", 12345);
		debug_log("Trying to print 64-bit integer: {}", 123456789012345LL);
		debug_log("Trying to print unsigned 32-bit integer: {}", 0x12345U);
		debug_log("Trying to print unsigned 64-bit integer: {}",
		          0x1234567809012345ULL);
		debug_log("Trying to print a float 123.456: {}", 123.456f);
		debug_log("Trying to print a double 123.456: {}", 123.456);
		debug_log("Trying to print function pointer {}",
		          compartment_error_handler);
		const char *testString = "Hello, world! with some trailing characters";
		// Make sure that we don't print the trailing characters
		debug_log("Trying to print std::string_view: {}",
		          std::string_view{testString, 13});
		const std::string S = "I am a walrus"s;
		debug_log("Trying to print std::string: {}", S);
		// Test stack pointer recovery in the root compartment.
		CHERI::Capability<void> csp{__builtin_cheri_stack_get()};
		CHERI::Capability<void> originalCSP{switcher_recover_stack()};
		csp.address() = originalCSP.address();
		TEST(csp == originalCSP,
		     "Original stack pointer: {}\ndoes not match current stack "
		     "pointer: {}",
		     originalCSP,
		     csp);
	}

#ifndef CLANG_TIDY
#	include "tests-all.inc"
#endif

	TEST(crashDetected == false, "One or more tests failed");

	simulation_exit();
}

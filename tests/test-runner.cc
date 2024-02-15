// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "tests.hh"
#include <compartment.h>
#include <simulator.h>

using namespace CHERI;

namespace
{
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
#elifdef IBEX
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
		int startCycles = rdcycle();
		fn();
		int cycles = rdcycle();
		debug_log("{} finished in {} cycles", msg, cycles - startCycles);
	}
} // namespace

/// Have we detected a crash in any of the compartments?
volatile bool crashDetected = false;

extern "C" enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame, size_t mcause, size_t mtval)
{
	if (mcause == 0x2)
	{
		debug_log("Test failure in test runner");
#ifdef SIMULATION
		simulation_exit(1);
#endif
		return ErrorRecoveryBehaviour::ForceUnwind;
	}
	debug_log("Current test crashed");
	crashDetected = true;
	return ErrorRecoveryBehaviour::InstallContext;
}

/**
 * Test suite entry point.  Runs all of the tests that we have defined.
 */
void __cheri_compartment("test_runner") run_tests()
{
	// magic_enum is a pretty powerful stress-test of various bits of linkage.
	// In generating `enum_values`, it generates constant strings and pointers
	// to them in COMDAT sections.  These require merging across compilation
	// units during our first link stage and then require cap relocs so that
	// they are all correctly bounded.  This works as a good smoke test of our
	// linker script.
	debug_log("Checking that rel-ro caprelocs work.  This will crash if they "
	          "don't.  CHERI Permissions are:");
	for (auto &permission : magic_enum::enum_values<CHERI::Permission>())
	{
		debug_log("{}", permission);
	}

	/*
	 * Add a test to check the PermissionSet Iterator. We swap the permissions
	 * order to match it to the order the Iterator implements: the Iterator
	 * returns then in value order, and not insertion order.
	 */
	static constexpr PermissionSet Permissions{
	  Permission::Load, Permission::Store, Permission::LoadStoreCapability};
	static Permission permissionArray[] = {
	  Permission::Store, Permission::Load, Permission::LoadStoreCapability};
	size_t index = 0;
	for (auto permission : Permissions)
	{
		TEST(permission == permissionArray[index++],
		     "Iterator of PermissionSet failed");
	}
	// These need to be checked visually
	debug_log("Trying to print 8-bit integer: {}", uint8_t(0x12));
	debug_log("Trying to print unsigned 8-bit integer: {}", int8_t(34));
	debug_log("Trying to print char: {}", 'c');
	debug_log("Trying to print 32-bit integer: {}", 12345);
	debug_log("Trying to print 64-bit integer: {}", 123456789012345LL);
	debug_log("Trying to print unsigned 32-bit integer: {}", 0x12345U);
	debug_log("Trying to print unsigned 64-bit integer: {}",
	          0x123456789012345ULL);
	const char *testString = "Hello, world! with some trailing characters";
	// Make sure that we don't print the trailing characters
	debug_log("Trying to print string: {}", std::string_view{testString, 13});

	run_timed("All tests", []() {
		run_timed("MMIO", test_mmio);
		run_timed("Static sealing", test_static_sealing);
		run_timed("Crash recovery", test_crash_recovery);
		run_timed("Compartment calls", test_compartment_call);
		run_timed("check_pointer", test_check_pointer);
		run_timed("Stacks exhaustion in the switcher", test_stack);
		run_timed("Thread pool", test_thread_pool);
		run_timed("Global Constructors", test_global_constructors);
		run_timed("Queue", test_queue);
		run_timed("Futex", test_futex);
		run_timed("Locks", test_locks);
		run_timed("Event groups", test_eventgroup);
		run_timed("Multiwaiter", test_multiwaiter);
		run_timed("Allocator", test_allocator);
	});

	TEST(crashDetected == false, "One or more tests failed");

	// Exit the simulator if we are running in simulation.
#ifdef SIMULATION
	simulation_exit();
#endif
	// Infinite loop if we're not in simulation.
	while (true)
	{
		yield();
	}
}

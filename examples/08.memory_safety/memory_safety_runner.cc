// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Memory safety (main runner)"
#include "memory_safety.h"
#include <cheri.hh>
#include <debug.hh>
#include <errno.h>
#include <fail-simulator-on-error.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Memory safety compartment">;

// Import some useful things from the CHERI namespace.
using namespace CHERI;

void __cheri_compartment("memory_safety_runner") entry()
{
	Debug::log("Demonstrate memory safety");

	int ret = 0;
	magic_enum::enum_for_each<MemorySafetyBugClass>(
	  [](MemorySafetyBugClass bug) {
		  int ret = memory_safety_inner_entry(bug);
		  Debug::Assert(ret == -1, "{} operation should have crashed", bug);
	  });
}

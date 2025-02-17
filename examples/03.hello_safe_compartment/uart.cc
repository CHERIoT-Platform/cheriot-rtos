// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "hello.h"
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <futex.h>
#include <string_view>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "UART compartment">;

// Import some useful things from the CHERI namespace.
using namespace CHERI;

/// Write a message to the UART.
int write(const char *msg)
{
	// Word containing the lock, this is 0 for unlocked, 1 for locked.
	static uint32_t lockWord = 0;
	with_interrupts_disabled([]() {
		// Check the word is 0, if it isn't then yield.  We may be woken after
		// another thread that manages to get the lock and so we need to retry
		// in a loop.
		while (lockWord != 0)
		{
			futex_wait(&lockWord, 0);
		}
		lockWord = 1;
	});
	// Make sure that this is a valid readable capability.
	if (check_pointer<PermissionSet{Permission::Load}>(msg))
	{
		// Don't assume that there's a null terminator.
		std::string_view message{msg,
		                         static_cast<size_t>(Capability{msg}.bounds())};
		Debug::log("{}", message);
	}
	// Release the lock.
	lockWord = 0;
	Debug::Invariant(futex_wake(&lockWord, -1) >= 0,
	                 "Compartment call to futex_wake failed");
	return 0;
}

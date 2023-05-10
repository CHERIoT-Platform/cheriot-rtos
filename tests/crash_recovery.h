// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "tests.hh"
#include <cdefs.h>

__cheri_compartment("crash_recovery_inner") void *test_crash_recovery_inner(
  int);
__cheri_compartment("crash_recovery_outer") void test_crash_recovery_outer(int);

/**
 * Checks that the stack is entirely full of zeroes below the current stack pointer.
 */
inline void check_stack(SourceLocation loc = SourceLocation::current())
{
	register char *cspRegister asm("csp");
	asm("" : "=C"(cspRegister));
	CHERI::Capability<char> csp{cspRegister};
	const size_t            stackAddress = csp.address();
	const size_t            length       = stackAddress - csp.base();
	csp.address()                        = csp.base();
	ssize_t failAddress                  = -1;
	// Scan the stack from the current stack pointer downwards and report the first non-zero value.
	// We must not call any functions in this loop or we would move the stack
	// pointer and write some non-zero values.
	for (ssize_t i = length - 1; i > 0; i--)
	{
		if (csp[i] != 0)
		{
			if (failAddress != -1)
			{
				failAddress = csp.address() + i;
			}
		}
	}
	Test::Invariant<size_t, void *, size_t, unsigned>(
	  failAddress == -1,
	  "Byte at {} in {} (stack address: {}) is {}, not 0",
	  csp.address() + failAddress,
	  csp,
	  stackAddress,
	  csp[failAddress],
	  loc);
}

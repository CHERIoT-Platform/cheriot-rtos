// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "tests.hh"
#include <cdefs.h>

__cheri_compartment("crash_recovery_inner") void *test_crash_recovery_inner(
  int);
__cheri_compartment("crash_recovery_outer") int test_crash_recovery_outer(int);

/**
 * Checks that the stack is entirely full of zeroes below the current stack
 * pointer.
 */
inline void check_stack(SourceLocation loc = SourceLocation::current())
{
	register char *cspRegister asm("csp");
	asm("" : "=C"(cspRegister));
	CHERI::Capability<char> csp{cspRegister};
	const ptraddr_t         StackAddress = csp.address();
	const size_t            Length       = StackAddress - csp.base();
	csp.address()                        = csp.base();

	// Scan the stack from the current stack pointer downwards and report the
	// first non-zero value. We must not call any functions in this loop or we
	// would move the stack pointer and write some non-zero values.
	ptrdiff_t failOffset = -1;
	for (ptrdiff_t i = Length - 1; i > 0; i--)
	{
		if (csp[i] != 0)
		{
			failOffset = i;
			break;
		}
	}

	/*
	 * Do a somewhat awkward dance to quiet clang-tidy's complaint that we might
	 * be about to compute and dereference csp[-1].
	 */
	ptrdiff_t debugOffset = (failOffset == -1) ? 0 : failOffset;
	Test::Invariant<char *, unsigned char>(failOffset == -1,
	                                       "Byte at {} is {}, not 0",
	                                       &csp[debugOffset],
	                                       csp[debugOffset],
	                                       loc);
}

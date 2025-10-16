// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>

/// Read the least significant word of the cycle counter.
__always_inline static int rdcycle()
{
	int cycles;
	// rdcycle is just "csrr %0, cycle" (CSR 0xC00, not mcycle (0xB00)).
	__asm__ volatile("rdcycle %0" : "=r"(cycles));
	return cycles;
}

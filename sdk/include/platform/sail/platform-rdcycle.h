// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>

/**
 * Read the least significant word of something like a "cycle" counter.
 *
 * On sail, this reports the number of instructions retired; the cycle count is
 * meaningless.
 */
__always_inline static int rdcycle()
{
	int cycles;
	__asm__ volatile("csrr %0, minstret" : "=r"(cycles));
	return cycles;
}

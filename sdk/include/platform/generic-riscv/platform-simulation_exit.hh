// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef SIMULATION
#	include <stdint.h>
#	ifdef RISCV_HTIF
/**
 * This is a special MMIO register. Writing an LSB of 1 terminates
 * simulation. The upper 31 bits can pass extra metadata. We use all 0s
 * to indicates success.
 *
 * This symbol doesn't need to be exported. The simulator is clever
 * enough to find it even if it's local.
 *
 * HTIF is more general than our use here, so it's possible that someone else
 * wants to use it as well, so we mark our symbol as weak.
 */
volatile uint32_t tohost[2] __attribute__((weak));
#	endif

static void platform_simulation_exit(uint32_t code)
{
#	ifdef RISCV_HTIF
	// If this is using the standard RISC-V to-host mechanism, this will exit.
	tohost[0] = 0x1 | (code << 1);
	tohost[1] = 0;
#	endif
}
#endif

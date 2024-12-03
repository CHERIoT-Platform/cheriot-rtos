// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef SIMULATION
#	include <stdint.h>

static void platform_simulation_exit(uint32_t code)
{
#	ifdef IBEX_SAFE
	// If we didn't exit with to-host, try writing a non-ASCII character to the
	// UART.  This is how we exit the CHERIoT Ibex simulator for the SAFE
	// platform.
	MMIO_CAPABILITY(Uart, uart)->blocking_write(0x80 + code);
#	endif
}
#endif

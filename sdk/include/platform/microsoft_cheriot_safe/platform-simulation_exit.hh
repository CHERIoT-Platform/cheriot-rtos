// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef SIMULATION
#	include <platform-uart.hh>
#	include <stdint.h>

static void platform_simulation_exit(uint32_t code)
{
	// Microsoft's SAFE platform exits if UART 0 writes above ASCII
	MMIO_CAPABILITY(Uart, uart)->blocking_write(0x80 + code);
}
#endif

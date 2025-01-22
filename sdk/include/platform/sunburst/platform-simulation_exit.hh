// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef SIMULATION
#	include <stdint.h>
#	include <platform-uart.hh>
#	include <string_view>

static void platform_simulation_exit(uint32_t code)
{
	auto uart =
#	if DEVICE_EXISTS(uart0)
	  MMIO_CAPABILITY(Uart, uart0);
#	elif DEVICE_EXISTS(uart)
	  MMIO_CAPABILITY(Uart, uart);
#	else
#		error No UART found in platform_simulation_exit
#	endif
	// Writing the following magic string to the UART will cause the sonata
	// simulator to exit.
	const char *magicString =
	  "Safe to exit simulator.\xd8\xaf\xfb\xa0\xc7\xe1\xa9\xd7";
	while (char ch = *magicString++)
	{
		uart->blocking_write(ch);
	}
}
#endif

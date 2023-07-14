// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <platform/ibex/platform-dma.hh>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Simple DMA request compartment">;

/// Thread entry point.
void __cheri_compartment("dma") dma_request()
{
	// Print hello world, along with the compartment's name to the default UART.
	Debug::log("DMA request app entered!");

	
}

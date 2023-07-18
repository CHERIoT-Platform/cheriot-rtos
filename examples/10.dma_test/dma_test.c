// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <utils.hh>

#include <fail-simulator-on-error.h>
#include <../core/dma/dma.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Simple DMA request compartment">;

/// Thread entry point.
void __cheri_compartment("dma") dma_request()
{
	Debug::log("DMA request app entered!");

	// This is a dma process between two different memory addresses
	uint32_t *sourceAddress =(uint32_t*) malloc(1024);
	uint32_t *targetAddress =(uint32_t*) malloc(1024);
	
	DMA::Device dmaDevice;

	dmaDevice.configure_and_launch(sourceAddress, targetAddress, 1024, 0, 0, 0);
}

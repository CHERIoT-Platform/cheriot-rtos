// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <utils.hh>

#include <fail-simulator-on-error.h>
#include <../../sdk/core/dma/dma.h>

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Simple DMA request compartment">;

/// Thread entry point.
void __cheri_compartment("dma_app") dma_request()
{
	Debug::log("DMA request app entered!");

	// This is a dma process between two different memory addresses
	uint32_t *sourceAddress =(uint32_t*) malloc(4);
	uint32_t *targetAddress =(uint32_t*) malloc(4);

	// for (int i=0; i<4; i++)
	// {
	// 	*(sourceAddress + i) = i + 100;
	// 	*(targetAddress + i) = i + 200;
	// }

	Debug::log("Source Address BEFORE: {}", *(sourceAddress));
	Debug::log("Target Address BEFORE: {}", *(sourceAddress));

	DMA::Device dmaDevice;

	int ret = dmaDevice.configure_and_launch(sourceAddress, targetAddress, 4, 0, 0, 0);
	
	Debug::log("Source Address AFTER: {}", *(sourceAddress));
	Debug::log("Target Address AFTER: {}", *(sourceAddress));

	Debug::log("ret: {}", ret);
}

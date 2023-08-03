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
	uint32_t *sourceAddress =(uint32_t*) malloc(16);
	uint32_t *targetAddress =(uint32_t*) malloc(16);

	for (int i=0; i<4; i++)
	{
		*(sourceAddress + i) = i + 100;
		*(targetAddress + i) = 0;

		Debug::log("Ind: {}, Source values BEFORE dma: {}", i, *(sourceAddress + i));
		Debug::log("Ind: {}, Dest-n values BEFORE dma: {}", i, *(targetAddress + i));
	}

	DMA::Device dmaDevice;

	int ret = dmaDevice.configure_and_launch(sourceAddress, targetAddress, 16, 0, 0, 0);

	for (int i=0; i<4; i++)
	{
		Debug::log("Ind: {}, Source values AFTER dma: {}", i, *(sourceAddress + i));
		Debug::log("Ind: {}, Dest-n values AFTER dma: {}", i, *(targetAddress + i));
	};

	Debug::log("ret: {}", ret);
}

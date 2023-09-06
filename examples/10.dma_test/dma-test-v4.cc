// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
#define MALLOC_QUOTA 0x10000000

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <utils.hh>

#include <../../sdk/core/dma-v4/dma.h>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "DMA Compartment">;

// Thread entry point.
void __cheri_compartment("dma_test") test_dma()
{
	Debug::log("DMA app entered, v4.0!");

	// This is a dma process between two different memory addresses
	uint32_t bytes    = 8096;
	uint32_t words    = bytes / 4;
	uint32_t byteSwap = 0;

	uint32_t *sourceAddress    = (uint32_t *)malloc(bytes);
	uint32_t *targetAddress    = (uint32_t *)malloc(bytes);

	for (int i = 0; i < words; i++)
	{
		*(sourceAddress + i)    = i + 100;
		*(targetAddress + i)    = 0;
	}

	Debug::log("M:Ind: 0 and last, Source values BEFORE dma: {}, {}",
	           *(sourceAddress),
	           *(sourceAddress + words - 1));
	Debug::log("M: Ind: 0 and last, Dest-n values BEFORE dma: {}, {}",
	           *(targetAddress),
	           *(targetAddress + words - 1));

	static DMA::Device dmaDevice;

	int ret = dmaDevice.configure_and_launch(
	sourceAddress, targetAddress, bytes, 0, 0, byteSwap);

	Debug::log("Main, ret: {}", ret);

	Debug::log("M: Ind: 0 and last, Dest-n values AFTER dma: {}, {}",
	           *(targetAddress),
	           *(targetAddress + words - 1));
	
	Debug::log("M: End of test");
}

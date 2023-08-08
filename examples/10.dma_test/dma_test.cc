// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <utils.hh>

#include <fail-simulator-on-error.h>
#include <../../sdk/core/dma/dma.h>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "DMA Compartment">;

/// Thread entry point.
void __cheri_compartment("dma_app") dma_request()
{
	Debug::log("DMA app entered!");

	// This is a dma process between two different memory addresses
	uint32_t bytes = 1024;
	uint32_t words = bytes/4;
	uint32_t byteSwap = 4;

	uint32_t *sourceAddress =(uint32_t*) malloc(bytes);
	uint32_t *targetAddress =(uint32_t*) malloc(bytes);

	for (int i=0; i < words; i++)
	{
		*(sourceAddress + i) = i + 200;
		*(targetAddress + i) = 0;

	}

	Debug::log("Ind: 0 and last, Source values BEFORE dma: {}, {}", *(sourceAddress), *(sourceAddress + words -1 ));
	Debug::log("Ind: 0 and last, Dest-n values BEFORE dma: {}, {}", *(targetAddress),  *(targetAddress + words -1 ));

	DMA::Device dmaDevice;

	int ret = dmaDevice.configure_and_launch(sourceAddress, targetAddress, bytes, 0, 0, byteSwap);

	Debug::log("Ind: 0 and last, Source values AFTER dma: {}, {}", *(sourceAddress), *(sourceAddress + words -1 ));
	Debug::log("Ind: 0 and last, Dest-n values AFTER dma: {}, {}", *(targetAddress),  *(targetAddress + words -1 ));

	Debug::log("ret: {}", ret);

	ret = dmaDevice.configure_and_launch(targetAddress, sourceAddress, bytes, 0, 0, 0);

	Debug::log("Ind: 0 and last, Source values AFTER dma: {}, {}", *(sourceAddress), *(sourceAddress + words -1 ));
	Debug::log("Ind: 0 and last, Dest-n values AFTER dma: {}, {}", *(targetAddress),  *(targetAddress + words -1 ));

	Debug::log("ret: {}", ret);
}

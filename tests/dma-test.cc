// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
#define TEST_NAME "DMA"
#include "tests.hh"
#include <cheri.hh>

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <utils.hh>

#include <../../sdk/core/dma-v2/dma.h>

#include <thread.h>
#include <thread_pool.h>

using namespace thread_pool;

// Thread entry point.
void test_dma()
{
	debug_log("DMA app entered, v2.10!");

	// This is a dma process between two different memory addresses
	uint32_t bytes    = 1024;
	uint32_t words    = bytes / 4;
	uint32_t byteSwap = 0;

	uint32_t *sourceAddress    = (uint32_t *)malloc(bytes);
	uint32_t *targetAddress    = (uint32_t *)malloc(bytes);
	uint32_t *alternateAddress = (uint32_t *)malloc(bytes);

	for (int i = 0; i < words; i++)
	{
		*(sourceAddress + i)    = i + 100;
		*(alternateAddress + i) = i + 200;
		*(targetAddress + i)    = 0;
	}

	debug_log("M:Ind: 0 and last, Source values BEFORE dma: {}, {}",
	           *(sourceAddress),
	           *(sourceAddress + words - 1));
			   
	debug_log("M:Ind: 0 and last, Source values BEFORE dma: {}, {}",
	           *(alternateAddress),
	           *(alternateAddress + words - 1));

	debug_log("M: Ind: 0 and last, Dest-n values BEFORE dma: {}, {}",
	           *(targetAddress),
	           *(targetAddress + words - 1));

	static DMA::Device dmaDevice;

	async([=]() {
		debug_log("Thread 1, start");

		int ret = dmaDevice.configure_and_launch(
		  sourceAddress, targetAddress, bytes, 0, 0, byteSwap);

		debug_log("Thread 1, ret: {}", ret);
	});

	// async([=]() {
	// 	debug_log("Thread Free, start");

	// 	free(sourceAddress);
	// });

	// async([=]() {
	// 	debug_log("Thread 2, start");
		
	// 	int ret = dmaDevice.configure_and_launch(
	// 	  alternateAddress, targetAddress, bytes, 0, 0, 0);

	// 	debug_log("Thread 2, ret: {}", ret);
	// });
	
	// here, we are just forcing to sleep
	// however, for experimental numbers 
	// we need to make sure to make a fare analysis
	Timeout t{100};
	thread_sleep(&t);
	
	// debug_log("Ind: 0 and last, Source values AFTER dma: {}, {}",
	// 	           *(sourceAddress),
	// 	           *(sourceAddress + words - 1));
	// debug_log("Ind: 0 and last, Source values AFTER dma: {}, {}",
	// 	           *(alternateAddress),
	// 	           *(alternateAddress + words - 1));
	debug_log("M: Ind: 0 and last, Dest-n values AFTER dma: {}, {}",
	           *(targetAddress),
	           *(targetAddress + words - 1));

	debug_log("M: End of test");
}

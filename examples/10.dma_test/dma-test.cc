// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <utils.hh>

#include <../../sdk/core/dma/dma.h>
// #include <fail-simulator-on-error.h>

#include <thread.h>
#include <thread_pool.h>

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "DMA Compartment">;

using namespace thread_pool;

#include <../../tests/crash_recovery.h>

using DebugErrorHandler = ConditionalDebug<true, "Error handler">;

int crashes = 0;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame, size_t mcause, size_t mtval)
{
	Debug::log("Test saw error for PCC {}", frame->pcc);
	Debug::log("Error cause: {}, mtval: {}", mcause, mtval);
	if (mcause == 0x2)
	{
		Debug::log("Test hit assertion failure, unwinding");
		return ErrorRecoveryBehaviour::ForceUnwind;
	}

	crashes++;
	Debug::log("Resuming test at failure location");
	return ErrorRecoveryBehaviour::InstallContext;
}

// Thread entry point.
void __cheri_compartment("dma_test") test_dma()
{
	Debug::log("DMA app entered, v2!");

	// This is a dma process between two different memory addresses
	uint32_t bytes    = 1024;
	uint32_t words    = bytes / 4;
	uint32_t byteSwap = 4;

	uint32_t *sourceAddress    = (uint32_t *)malloc(bytes);
	uint32_t *targetAddress    = (uint32_t *)malloc(bytes);
	uint32_t *alternateAddress = (uint32_t *)malloc(bytes);

	for (int i = 0; i < words; i++)
	{
		*(sourceAddress + i)    = i + 100;
		*(alternateAddress + i) = i + 200;
		*(targetAddress + i)    = 0;
	}

	Debug::log("M:Ind: 0 and last, Source values BEFORE dma: {}, {}",
	           *(sourceAddress),
	           *(sourceAddress + words - 1));
	Debug::log("M: Ind: 0 and last, Dest-n values BEFORE dma: {}, {}",
	           *(targetAddress),
	           *(targetAddress + words - 1));

	// static DMA::Device dmaDevice;

	async([=]() {
		Debug::log("Thread 1, start");

		// int ret = dmaDevice.configure_and_launch(
		//   sourceAddress, targetAddress, bytes, 0, 0, byteSwap);

		// Debug::log("Ind: 0 and last, Source values AFTER dma: {}, {}",
		//            *(sourceAddress),
		//            *(sourceAddress + words - 1));
		// Debug::log("Ind: 0 and last, Dest-n values AFTER dma: {}, {}",
		//            *(targetAddress),
		//            *(targetAddress + words - 1));

		// Debug::log("Thread 1, ret: {}", ret);
	});

	async([=]() {
		Debug::log("Thread 2, start");

		// int ret = dmaDevice.configure_and_launch(
		//   alternateAddress, targetAddress, bytes, 0, 0, 0);

		// Debug::log("Ind: 0 and last, Source values AFTER dma: {}, {}",
		//            *(alternateAddress),
		//            *(alternateAddress + words - 1));
		// Debug::log("Ind: 0 and last, Dest-n values AFTER dma: {}, {}",
		//            *(targetAddress),
		//            *(targetAddress + words - 1));

		// Debug::log("Thread 2, ret: {}", ret);
	});

	Debug::log("M: Ind: 0 and last, Dest-n values AFTER dma: {}, {}",
	           *(targetAddress),
	           *(targetAddress + words - 1));
	Debug::log("M: End of test");
}

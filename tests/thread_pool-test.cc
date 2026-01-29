// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cstdint>
#define TEST_NAME "Thread pool"
#include "tests.hh"
#include <cheri.hh>
#include <cheriot-atomic.hh>
#include <switcher.h>
#include <thread.h>
#include <thread_pool.h>

int counter;

using CHERI::with_interrupts_disabled;
using namespace thread_pool;

cheriot::atomic<bool> errorHandled     = false;
cheriot::atomic<bool> interruptStarted = false;
cheriot::atomic<int>  interruptThreadNumber;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("Thread {} error handler invoked with mcause {}.  PCC: {}",
	          thread_id_get(),
	          mcause,
	          frame->pcc);
	if (mcause != 25)
	{
		return ErrorRecoveryBehaviour::ForceUnwind;
	}
	if (thread_id_get() != interruptThreadNumber)
	{
		debug_log(
		  "Explicit thread interrupt delivered on the wrong thread (thread {}, "
		  "expected {})",
		  thread_id_get(),
		  interruptThreadNumber.load());
		return ErrorRecoveryBehaviour::ForceUnwind;
	}
	errorHandled = true;
	debug_log("Expected software interrupt, installing context");
	return ErrorRecoveryBehaviour::InstallContext;
}

int test_thread_pool()
{
	// We can't share stack variables, so create a heap allocation that we can
	// capture as an explicit pointer.
	int *heapInt = new (malloc(sizeof(int))) int(0);
	TEST(thread_id_get() == 1,
	     "Thread id of main thread should be 1, is {}",
	     thread_id_get());
	// Run a simple stateless callback that increments a global in the thread
	// pool.  This demonstrates that we can correctly capture a stateless
	// function and pass it to the worker thread.
	async([]() {
		with_interrupts_disabled([]() {
			counter++;
			debug_log("Calling stateless function from thread pool");
		});
	});
	async([=]() {
		with_interrupts_disabled([=]() {
			debug_log(
			  "Calling stateful function from thread pool with {} captured",
			  heapInt);
			counter++;
			(*heapInt)++;
		});
	});
	debug_log("Counter: {}", counter);
	debug_log("heapInt: {}", *heapInt);
	int sleeps = 0;
	while (counter < 2)
	{
		TEST(sleep(1) >= 0, "Failed to sleep");
		TEST(sleeps < 100, "Gave up after too many sleeps");
	}
	debug_log("Yielded {} times for the thread pool to run our jobs", sleeps);
	TEST(counter == 2, "Counter is {}, should be 2", counter);
	TEST(*heapInt == 1, "Heap-allocated integer is {}, should be 1", *heapInt);
	debug_log("Freeing heap int: {}", heapInt);
	free(heapInt);

	async([]() {
		TEST(thread_id_get() != 1,
		     "Thread ID for thread pool thread should not be 1");
	});

	return 0;
}

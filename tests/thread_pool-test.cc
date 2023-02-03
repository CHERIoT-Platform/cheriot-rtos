// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Thread pool"
#include "tests.hh"
#include <cheri.hh>
#include <thread.h>
#include <thread_pool.h>

int counter;

using CHERI::with_interrupts_disabled;
using namespace thread_pool;

void test_thread_pool()
{
	// We can't share stack variables, so create a heap allocation that we can
	// capture as an explicit pointer.
	int *heapInt = new (malloc(sizeof(int))) int(0);
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
		Timeout t{1};
		thread_sleep(&t);
		TEST(sleeps < 100, "Gave up after too many sleeps");
	}
	debug_log("Yielded {} times for the thread pool to run our jobs", sleeps);
	TEST(counter == 2, "Counter is {}, should be 2", counter);
	TEST(*heapInt == 1, "Heap-allocated integer is {}, should be 1", *heapInt);
	debug_log("Freeing heap int: {}", heapInt);
	free(heapInt);
}

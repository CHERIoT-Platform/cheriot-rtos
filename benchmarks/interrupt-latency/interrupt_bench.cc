#include "../timing.h"
#include <compartment.h>
#include <debug.hh>
#include <locks.hh>
#include <simulator.h>
#include <thread.h>

/**
 * \file benchmark that measures the time to wake from waiting on a futex
 *
 * This benchmark is intended to measure the interrupt wake latency without an
 * actual interrupt by measuring the time taken to wake from a call to
 * futex_wait. This is relevant because CHERIoT RTOS exposes interrupts as
 * counters that can be waited on and will be incremented and notified by the
 * OS. On platforms with an easy to trigger external interrupt available use the
 * interrupt-latency-ext benchmark instead.
 */

#ifndef DEBUG_INTERRUPT_BENCH
#	define DEBUG_INTERRUPT_BENCH false
#endif

#if DEBUG_INTERRUPT_BENCH
#	include <fail-simulator-on-error.h>
#endif

using Debug = ConditionalDebug<DEBUG_INTERRUPT_BENCH, "Interrupt benchmark">;

namespace
{
	std::atomic<uint32_t> start;
} // namespace

/**
 * N threads of equal priority will enter here with different stack sizes (the
 * stack size actually makes no difference, but was used as an experimental
 * variable). They will all wait on a ticket lock so that only one of them runs
 * at a time. The first one to run will write a TSV header and they will each
 * wait on the atomic variable `start` which will be set and notified by the
 * low-priority thread. When they wake up they read the cycle counter to see how
 * much time has elapsed since the low priority thread notified `start`, then
 * print the result and exit, release the ticket lock.
 */
int __cheri_compartment("interrupt_bench") entry_high_priority()
{
	static TicketLock       lock;
	static _Atomic(uint8_t) threadCounter = 0;

	uint16_t threadID = thread_id_get();
	if (++threadCounter == 1)
	{
		Debug::log("Thread {} writing header", threadID);
		printf("#board\tstack size\ttotal\n");
	}

	Debug::log("Thread {} entering ticket lock", threadID);
	LockGuard g{lock};
	Debug::log("Thread {} got ticket lock", threadID);

	// Note: pre-emption is not possible from here because this is the only
	// runnable thread at this priority (and the scheduler is tickless),
	// therefore there is no need to disable interrupts.

	Debug::log("Thread {} waiting on event", threadID);
	start.wait(start);
	int    end       = rdcycle();
	size_t stackSize = get_stack_size();
	printf(__XSTRING(BOARD) "\t%d\t%d\n", stackSize, end - start);

	// Last one out turns off the lights. This relies on all threads
	// incrementing the counter before any thread reaches here. We can be sure
	// of this because for any thread to reach here the low priority thread has
	// to run which can only happen when all high priority threads are waiting
	// and the first blocking event (LockGuard constructor) is after the counter
	// increment.
	if (--threadCounter == 0)
	{
		Debug::log("Thread {} exiting simulator", threadID);
		simulation_exit(0); // Never returns
		__builtin_unreachable();
	}

	Debug::log("Thread {} releasing ticket lock (exiting)", threadID);
	return 0;
}

/**
 * This lower priority thread will run once all the higher priority threads are
 * waiting on the ticket lock or `start`: it can't run earlier because one of
 * the higher priority threads will always be runnable until that point. It sets
 * `start` to the current value of the cycle counter then notifies it, which
 * will do a futex_notify() simulating an interrupt waking the waiting thread.
 * That thread will be run immediately due to being higher priority and it will
 * then read the cycle counter again to calculate the futex_wake latency and
 * exit. We repeat this until all the higher priority threads have run, with the
 * last one calling simulation_exit to exit the simulator if applicable.
 */
int __cheri_compartment("interrupt_bench") entry_low_priority()
{
	while (true)
	{
		Debug::log("Low thread setting event");
		start = rdcycle();
		// We could use notify_one here which results in slightly lower latency.
		// Even though there is only one thread waiting on this futex the
		// scheduler walks the list of all threads waiting on futexes which
		// includes the threads waiting on the ticket lock. However since the
		// scheduler uses notify_all on interrupt futexes we use that to more
		// closely emulate an interrupt wake.
		start.notify_all();
	}
	return 0;
}

#include "../timing.h"
#include <compartment.h>
#include <debug.hh>
#include <event.h>
#include <locks.hh>
#include <simulator.h>
#include <thread.h>
#include <timeout.h>
#if DEBUG_INTERRUPT_BENCH
#	include <fail-simulator-on-error.h>
#endif

using Debug = ConditionalDebug<DEBUG_INTERRUPT_BENCH, "Interrupt benchmark">;

namespace
{
	std::atomic<uint32_t> event;
	int                   start;
} // namespace

/**
 * N threads of equal priority will enter here with different stack sizes. They
 * will all wait on a ticket lock so that only one of them runs at a time. They
 * will then wait on a futex that will be set by the low-priority thread.
 *
 * All of these threads will be waiting for the futex.
 */
void __cheri_compartment("interrupt_bench") entry_high_priority()
{
	Timeout                 t = {0, UnlimitedTimeout};
	static TicketLock       lock;
	static bool             headerWritten;
	static _Atomic(uint8_t) threadCounter = 0;
	threadCounter++;

	uint16_t threadID = thread_id_get();
	{
		Debug::log("Thread {} entering ticket lock", threadID);
		LockGuard g{lock};
		Debug::log("Thread {} got ticket lock", threadID);

		if (!headerWritten)
		{
			Debug::log("Thread {} creating event", threadID);
			printf("#board\tstack size\ttotal\n");
			headerWritten = true;
		}

		int    end       = CHERI::with_interrupts_disabled([&]() {
            uint32_t bits = 0;
            Debug::log("Thread {} releasing ticket lock", threadID);
            g.unlock();
            Debug::log("Thread {} waiting on event", threadID);
            event.wait(0);
            int time = rdcycle();
            Debug::Invariant(event == 1, "Futex woke spuriously");
            return time;
		         });
		size_t stackSize = get_stack_size();
		printf(__XSTRING(BOARD) "\t%d\t%d\n", stackSize, end - start);
	}

	// Last one out turns off the lights. This relies on all threads
	// incrementing the counter before any thread reaches here. We can be sure
	// of this because for any thread to reach here the low priority thread has
	// to run which can only happen when all high priority threads are waiting
	// and the first blocking event (LockGuard constructor) is after the counter
	// increment.
	if (--threadCounter == 0)
	{
		Debug::log("Thread {} exiting simulator", threadID);
		simulation_exit(0);
	}
	else
	{
		// Other threads sleep forever. we could exit (return) instead but this
		// seems to trigger a bug sometimes where the low priority thread
		// doesn't wake up.
		thread_sleep(&t);
	}
}

/**
 * This lower priority thread will run once all the higher priority threads are
 * waiting (either on the ticket lock or the event). It sets the event without
 * yielding, puts the starting cycle counter in a global then yields(), which
 * does an 'ecall' simulating an interrupt waking the waiting thread, which
 * reads the cycle counter again to calculate the interrupt latency. We repeat
 * this until all the higher priority threads have run, with the last one
 * calling exiting.
 */
void __cheri_compartment("interrupt_bench") entry_low_priority()
{
	while (true)
	{
		CHERI::with_interrupts_disabled([]() {
			uint32_t bits = 0;
			Debug::log("Low thread setting event");
			event = 1;
			start = rdcycle();
			event.notify_all();
		});
	}
}

#include "../timing.h"
#include <timeout.h>
#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <event.h>
#include <simulator.h>
#include <locks.hh>
#if DEBUG_INTERRUPT_BENCH
#include <fail-simulator-on-error.h>
#endif

using Debug = ConditionalDebug<DEBUG_INTERRUPT_BENCH, "Interrupt benchmark">;

void *event_group;
int start;

/**
 * N threads of equal priority will enter here with different stack sizes. They
 * will all wait on a ticket lock so that only one of them runs at a time. The
 * first one creates an event group then they each wait on the event group in
 * turn and measure the latency from the time the low priority thread yields
 * until the higher priority thread returns from event_bits_wait.
 */
void __cheri_compartment("interrupt_bench") entry_high_priority()
{
	MessageBuilder<ImplicitUARTOutput> out;

	Timeout t = {0, UnlimitedTimeout};
	static TicketLock lock;
	static bool headerWritten;
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
			Debug::Invariant(event_create(&t, MALLOC_CAPABILITY, &event_group) == 0, "event create failed");
			out.format("#board\tstack size\ttotal\n");
			headerWritten = true;
		}

		int end = CHERI::with_interrupts_disabled([&]() {
			uint32_t bits = 0;
			Debug::log("Thread {} waiting on event", threadID);
			int ret = event_bits_wait(&t, event_group, &bits, 1, true, true);
			int time = rdcycle();
			Debug::Invariant(ret == 0, "event_bits_wait failed");
			return time;
		});
		size_t stackSize = get_stack_size();
		out.format(__XSTRING(BOARD) "\t{}\t{}\n", stackSize, end - start);
		Debug::log("Thread {} releasing ticket lock", threadID);
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
	while(true)
	{
		CHERI::with_interrupts_disabled([]() {
			uint32_t bits = 0;
			Debug::log("Low thread setting event");
			start = rdcycle();
			event_bits_set(event_group, &bits, 1);
		});
	}
}

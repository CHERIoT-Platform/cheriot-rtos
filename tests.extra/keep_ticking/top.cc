#include <debug.hh>
#include <thread.h>

using Debug = ConditionalDebug<true, "top">;

void tick_forever(int tid, Ticks period)
{
	unsigned int cycles;

	Debug::log("Entry {}...", tid);
	while (true)
	{
		Timeout t{period};
		int     res = thread_sleep(&t, ThreadSleepNoEarlyWake);
		__asm__ volatile("csrr %0, mcycle" : "=r"(cycles));
		Debug::log("At {}, thread {} sleep result {}; elapsed {} remaining {}",
		           cycles,
		           tid,
		           res,
		           t.elapsed,
		           t.remaining);
	}
}

void __cheri_compartment("top") entry1()
{
	tick_forever(1, 14);
}

void __cheri_compartment("top") entry2()
{
	tick_forever(2, 15);
}

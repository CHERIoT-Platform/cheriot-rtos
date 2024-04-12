// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <compartment.h>
#include <riscvreg.h>
#include <stddef.h>
#include <stdint.h>
#include <tick_macros.h>
#include <timeout.h>

__BEGIN_DECLS
/// the absolute system tick value since boot, 64-bit, assuming never overflows
typedef struct
{
	/// low 32 bits of the system tick
	uint32_t lo;
	/// hi 32 bits
	uint32_t hi;
} SystickReturn;
[[cheri::interrupt_state(disabled)]] SystickReturn __cheri_compartment("sched")
  thread_systemtick_get(void);

enum ThreadSleepFlags : uint32_t
{
	/**
	 * Sleep for up to the specified timeout, but wake early if there are no
	 * other runnable threads.  This allows a high-priority thread to yield for
	 * a fixed number of ticks for lower-priority threads to run, but does not
	 * prevent it from resuming early.
	 */
	ThreadSleepNoEarlyWake = 1 << 0,
};

/**
 * Sleep for at most the specified timeout (see `timeout.h`).
 *
 * The thread becomes runnable once the timeout has expired but a
 * higher-priority thread may prevent it from actually being scheduled.  The
 * return value is a saturating count of the number of ticks that have elapsed.
 *
 * A call of `thread_sleep` is with a timeout of zero is equivalent to `yield`,
 * but reports the time spent sleeping.  This requires a cross-domain call and
 * return in addition to the overheads of `yield` and so `yield` should be
 * preferred in contexts where the elapsed time is not required.
 *
 * The `flags` parameter is a bitwise OR of `ThreadSleepFlags`.
 *
 * A sleeping thread may be woken early if no other threads are runnable or
 * have earlier timeouts.  The thread with the earliest timeout will be woken
 * first.  This can cause a yielding thread to sleep when no other thread is
 * runnable, but avoids a potential problem where a high-priority thread yields
 * to allow a low-priority thread to make progress, but then the low-priority
 * thread does a short sleep.  In this case, the desired behaviour is not to
 * wake the high-priority thread early, but to allow the low-priority thread to
 * run for the full duration of the high-priority thread's yield.
 *
 * If you are using `thread_sleep` to elapse real time, pass
 * `ThreadSleepNoEarlyWake` as the flags argument to prevent early wakeups.
 */
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  thread_sleep(struct Timeout *timeout, uint32_t flags __if_cxx(= 0));

/**
 * Return the thread ID of the current running thread.
 * This is mostly useful where one compartment can run under different threads
 * and it matters which thread entered this compartment.
 *
 * This is implemented in the switcher.
 */
uint16_t __cheri_libcall thread_id_get(void);

/**
 * Returns the number of cycles accounted to the idle thread.
 *
 * This API is available only if the scheduler is built with accounting support
 * enabled.
 */
__cheri_compartment("sched") uint64_t thread_elapsed_cycles_idle(void);

/**
 * Returns the number of cycles accounted to the current thread.
 *
 * This API is available only if the scheduler is built with accounting
 * support enabled.
 */
__cheri_compartment("sched") uint64_t thread_elapsed_cycles_current(void);

/**
 * Returns the number of threads, including threads that have exited.
 *
 * This API never fails, but if the trusted stack is exhausted  and it cannot
 * be called then it will return -1.  Callers that have not probed the trusted
 * stack should check for this value.
 *
 * The result of this is safe to cache: it will never change over time.
 */
__cheri_compartment("sched") uint16_t thread_count();

/**
 * Wait for the specified number of microseconds.  This is a busy-wait loop,
 * not a yield.  If the thread is preempted then the wait will be longer than
 * requested.
 *
 * Returns the number of microseconds that the thread actually waited, with an
 * error margin of the number of instructions used to compute the wait time and
 * execute the function epilogue.
 */
static inline uint64_t thread_microsecond_spin(uint32_t microseconds)
{
#ifdef SIMULATION
	// In simulation builds, pretend that the right amount of time has elapsed.
	return microseconds;
#else
	static const uint32_t CyclesPerMicrosecond = CPU_TIMER_HZ / 1'000'000;
	__if_cxx(
	  static_assert(CyclesPerMicrosecond > 0, "CPU_TIMER_HZ is too low"););
	uint64_t start = rdcycle64();
	// Convert the microseconds to a number of cycles.  This does the multiply
	// first so that we don't end up with zero as a result of the division.
	uint32_t cycles = microseconds * CyclesPerMicrosecond;
	uint64_t end    = start + cycles;
	uint64_t current;
	do
	{
		current = rdcycle64();
	} while (current < end);
	return (current - start) * CyclesPerMicrosecond;
#endif
}

/**
 * Wait for the specified number of milliseconds.  This will yield for periods
 * that are longer than a scheduler tick and then spin for the remainder of the
 * time.
 *
 * Returns the number of milliseconds that the thread actually waited.
 */
static inline uint64_t thread_millisecond_wait(uint32_t milliseconds)
{
#ifdef SIMULATION
	// In simulation builds, just yield once but don't bother trying to do
	// anything sensible with time.
	Timeout t = {0, 1};
	thread_sleep(&t, 0);
	return milliseconds;
#else
	static const uint32_t CyclesPerMillisecond = CPU_TIMER_HZ / 1'000;
	static const uint32_t CyclesPerTick        = CPU_TIMER_HZ / TICK_RATE_HZ;
	__if_cxx(
	  static_assert(CyclesPerMillisecond > 0, "CPU_TIMER_HZ is too low"););
	uint32_t cycles  = milliseconds * CyclesPerMillisecond;
	uint64_t start   = rdcycle64();
	uint64_t end     = start + cycles;
	uint64_t current = start;
	while ((end > current) && (end - current > MS_PER_TICK))
	{
		Timeout t = {0, ((uint32_t)(end - current)) / CyclesPerTick};
		thread_sleep(&t, ThreadSleepNoEarlyWake);
		current = rdcycle64();
	}
	// Spin for the remaining time.
	while (current < end)
	{
		current = rdcycle64();
	}
	current = rdcycle64();
	return (current - start) / CyclesPerMillisecond;
#endif
}

__END_DECLS

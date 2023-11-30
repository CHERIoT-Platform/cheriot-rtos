// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <compartment.h>
#include <stddef.h>
#include <stdint.h>
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
 */
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  thread_sleep(struct Timeout *timeout);

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

__END_DECLS

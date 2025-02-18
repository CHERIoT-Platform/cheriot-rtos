// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cdefs.h>
#include <stdint.h>
#include <timeout.h>

enum [[clang::flag_enum]] FutexWaitFlags
{
	/// No flags
	FutexNone = 0,
	/**
	 * This futex uses priority inheritance.  The low 16 bits of the futex word
	 * are assumed to hold the thread ID of the thread that currently holds the
	 * lock.
	 */
	FutexPriorityInheritance = (1 << 0)
};

/**
 * Compare the value at `address` to `expected` and, if they match, sleep the
 * thread until a wake event is sent with `futex_wake` or until this the thread
 * has slept for `ticks` ticks.
 *
 * The value of `ticks` specifies the permitted timeout.  See `timeout.h` for
 * details.
 *
 * The `address` argument must permit loading four bytes of data after the
 * address.
 *
 * The `flags` argument contains flags that may control the behaviour of the
 * call.
 *
 * This returns:
 *
 *  - 0 on success: either `*address` and `expected` differ or a wake is
 *    received.
 *  - `-EINVAL` if the arguments are invalid.
 *  - `-ETIMEOUT` if the timeout expires.
 */
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  futex_timed_wait(Timeout        *ticks,
                   const uint32_t *address,
                   uint32_t        expected,
                   uint32_t flags  __if_cxx(= FutexNone));

/**
 * Compare the value at `address` to `expected` and, if they match, sleep the
 * thread until a wake event is sent with `futex_wake`.
 *
 * The `address` argument must permit loading four bytes of data after the
 * address.
 *
 * This returns 0 on success or `-EINVAL` if the arguments are invalid.
 */
__always_inline static int futex_wait(const uint32_t *address,
                                      uint32_t        expected)
{
	Timeout t = {0, UnlimitedTimeout};
	return futex_timed_wait(&t, address, expected, FutexNone);
}

/**
 * Wakes up to `count` threads that are sleeping with `futex[_timed]_wait` on
 * `address`.
 *
 * The `address` argument must permit storing four bytes of data after the
 * address. This call does not store to the address but requiring store
 * permission prevents a thread from waking up a futex that it cannot possibly
 * have moved to a different state.
 *
 * The return value for a successful call is the number of threads that were
 * woken.  `-EINVAL` is returned for invalid arguments.
 */
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  futex_wake(uint32_t *address, uint32_t count);

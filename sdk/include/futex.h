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
 * thread until a wake event is sent with `futex_wake` or until the thread has
 * slept for `ticks` ticks.
 *
 * The value of `ticks` specifies the permitted timeout.  See `timeout.h` for
 * details.
 *
 * The `address` argument must permit loading four bytes of data after the
 * address.
 *
 * The `flags` argument contains flags that may control the behaviour of the
 * call.  This is either `FutexNone` (zero) for the default behaviour or
 * `FutexPriorityInheritance` if the low 16 bits should be treated as a thread
 * ID for priority inheritance.
 *
 * This returns:
 *
 *  - 0 on success: either `*address` and `expected` differ or a wake is
 *    received.
 *  - `-EINVAL` if the arguments are invalid.
 *  - `-ETIMEDOUT` if the timeout expires.
 */
[[cheriot::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  futex_timed_wait(Timeout                 *ticks,
                   const volatile uint32_t *address,
                   uint32_t                 expected,
                   uint32_t flags           __if_cxx(= FutexNone));

/**
 * Compare the value at `address` to `expected` and, if they match, sleep the
 * thread until a wake event is sent with `futex_wake`.
 *
 * The `address` argument must permit loading four bytes of data after the
 * address.
 *
 * This returns 0 on success or `-EINVAL` if the arguments are invalid.
 */
__always_inline static int futex_wait(const volatile uint32_t *address,
                                      uint32_t                 expected)
{
	Timeout t = {0, UnlimitedTimeout};
	return futex_timed_wait(&t, address, expected, FutexNone);
}

/**
 * Wakes up to `count` threads that are sleeping with `futex_timed_wait` on
 * `address`.
 *
 * The `address` argument must be a valid unsealed pointer with a length of at
 * least four after the address but the scheduler does not require any explicit
 * permissions.  The scheduler never needs store access to the futex word.
 * Removing store permission means that a compromised scheduler can cause
 * spurious wakes but cannot tamper with the futex word.  If, for example, the
 * futex word is a lock then the scheduler can wake threads that are blocked on
 * the lock but cannot release the lock and so cannot make two threads believe
 * that they have simultaneously acquired the same lock.
 *
 * The return value for a successful call is the number of threads that were
 * woken.  `-EINVAL` is returned for invalid arguments.
 */
[[cheriot::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
  futex_wake(const volatile uint32_t *address, uint32_t count);

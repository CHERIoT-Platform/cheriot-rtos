// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cdefs.h>
#include <stdint.h>
#include <timeout.h>

/**
 * \file
 *
 * Futex (compare-and-wait) APIs.
 */

/**
 * Flags for `futex_timed_wait`.
 */
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
  futex_timed_wait(TimeoutArgument          timeout,
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
	return futex_timed_wait(TimeoutWaitForever, address, expected, FutexNone);
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

/**
 * Packaged common design pattern for awaiting an event signaled by a futex.
 * Repeatedly invokes `checkCallback(argument)` until it returns nonzero or
 * `futex_timed_wait(tiemout, futex, ...)` returns nonzero; this function
 * returns the first such nonzero value.
 *
 * This function calls `yield()` between the first `maximumYields` invocations
 * of `checkCallback(argument)`, allowing the system to run other threads at the
 * same priority while waiting for this event.  After that, this function will
 * call `enableCallback(argument)` and then subsequent invocations of
 * `checkCallback(argument)` returning zero will be followed by
 * `fuxex_timed_wait(timeout, futex, ...)`, with a futex value having been read
 * prior to the invocation of `checkCallback(argument)` in order to avoid missed
 * wakeups if `checkCallback` and/or `futex_wait` run concurrently with a
 * futex notifier.
 *
 * The `enableCallback` is expected to ensure that the given `futex` will
 * change at least when `checkCallback(argument)`'s return value would
 * (meaningfully) change.  This is especially useful when the futex is
 * specifically an interrupt futex, and, in this case,
 * `enableCallback(argument)` should configure the source hardware to deliver
 * the correct events.
 */
[[nodiscard]]
int __cheriot_libcall futex_await_poll(TimeoutArgument          timeout,
                                       uint16_t                 maximumYields,
                                       const volatile uint32_t *futex,
                                       void (*enableCallback)(void *),
                                       int (*checkCallback)(void *),
                                       void *argument);

/**
 * An always-inline wrapper around `futex_await_poll` which optionally
 * busy-spins some number of times.  Rarely preferred over `futex_await_poll`,
 * since futexes often depend on other threads making progress, this may still
 * be useful in the case that interrupt futexes are used to signal events that
 * often happen nearly immediately after the call but have a long tail of delays
 * where sleeping is desirable.
 */
[[nodiscard]]
static int __always_inline futex_await_poll_spin(TimeoutArgument timeout,
                                                 uint16_t        maximumSpins,
                                                 uint16_t        maximumYields,
                                                 const volatile uint32_t *futex,
                                                 void (*enableCallback)(void *),
                                                 int (*checkCallback)(void *),
                                                 void *argument)
{
	int res;

	for (uint16_t attempt = 0; attempt < maximumSpins; attempt++)
	{
		if ((res = checkCallback(argument)) != 0)
		{
			return res;
		}
	}

	return futex_await_poll(
	  timeout, maximumYields, futex, enableCallback, checkCallback, argument);
}

#ifdef __cplusplus

#	include <type_traits>

/**
 * A C++ wrapper around the C-compatible `futex_await_poll_spin`.  This takes
 * the maximum counts of spins and yields as template parameters and C++
 * callables for the `enableCallback` and `checkCallback`s.  Neither of these
 * callables take arguments; `enable` must have `void` return type and `check`
 * must have `int`.  These callables must remain live across the function call
 * (and will be in the typical case where they are on the caller's stack).
 */
template<uint16_t MaximumSpins  = 0,
         uint16_t MaximumYields = 0,
         typename Enable,
         typename Check>
    requires std::is_invocable_r_v<void, Enable> &&
	         std::is_invocable_r_v<int, Check>
[[nodiscard]]
static int __always_inline futex_await_poll_spin(TimeoutArgument timeout,
                                                 const volatile uint32_t *futex,
                                                 Enable &&enable,
                                                 Check  &&check)
{
	struct CArgument
	{
		Enable *enable;
		Check  *check;
	} cArgument{&enable, &check};

	return futex_await_poll_spin(
	  timeout,
	  MaximumSpins,
	  MaximumYields,
	  futex,
	  [](void *voidedArguments) {
		  auto arguments = static_cast<CArgument *>(voidedArguments);
		  (*static_cast<Enable *>(arguments->enable))();
	  },
	  [](void *voidedArguments) -> int {
		  auto arguments = static_cast<CArgument *>(voidedArguments);
		  return (*static_cast<Check *>(arguments->check))();
	  },
	  &cArgument);
}
#endif

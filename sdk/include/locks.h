#pragma once
#include <cdefs.h>
#include <limits>
#include <stdatomic.h>
#include <stdint.h>
#include <thread.h>
#include <timeout.h>

/**
 * State for a flag lock.  Flag locks use a single futex word to store the lock
 * state.
 */
struct FlagLockState
{
	/**
	 * The lock word.  One bit is used to indicate that the lock is held,
	 * another to indicate that the lock has waiters.  In priority-inheriting
	 * locks, the low 16 bits store the ID of the thread that currently holds
	 * the lock.
	 */
	_Atomic(uint32_t) lockWord __if_cxx(= 0);
};

/**
 * Ticket locks use two monotonic counters to store the lock state.
 */
struct TicketLockState
{
	/**
	 * The value for the current ticket holder.
	 */
	_Atomic(uint32_t) current __if_cxx(= 0);
	/**
	 * The value for the next ticket to be granted.
	 */
	_Atomic(uint32_t) next __if_cxx(= 0);
};

/**
 * State for a recursive mutex.  Recursive mutexes allow a single thread to
 * acquire the lock multiple times.
 */
struct RecursiveMutexState
{
	/**
	 * The underlying lock.
	 */
	FlagLockState lock;
	/**
	 * The count of the times the lock has been acquired by the same thread.
	 * This must be initialised to 0.
	 */
	uint16_t depth __if_cxx(= 0);
};

/**
 * State for a counting semaphore.
 */
struct CountingSemaphoreState
{
	/**
	 * The current counter value.
	 */
	_Atomic(uint32_t) count;

	/**
	 * The maximum value for the counter.
	 */
	uint32_t maxCount;
};

__BEGIN_DECLS

/**
 * Try to lock a flag lock.  This is the non-priority-inheriting version,
 * sometimes called a binary semaphore on other platforms.
 *
 * Returns 0 on success, -ETIMEDOUT if the timeout expired, or -EINVAL if the
 * arguments are invalid.
 */
int __cheri_libcall flaglock_trylock(Timeout *timeout, FlagLockState *lock);

/**
 * Try to lock a flag lock.  This is the priority-inheriting version.  Some
 * other platforms refer to this as a priority-inheriting mutex or simply a
 * mutex.  The `threadID` argument should be the current thread's ID.  In C++,
 * this should be omitted.
 *
 * A higher-priority thread that calls this function while a lock is held will
 * lend its priority to the thread that successfully called this function until
 * that thread either releases the lock with `flaglock_unlock` or the timeout
 * expires.
 *
 * Returns 0 on success, -ETIMEDOUT if the timeout expired, or -EINVAL if the
 * arguments are invalid.
 */
int __cheri_libcall flaglock_priority_inheriting_trylock(
  Timeout          *timeout,
  FlagLockState    *lock,
  uint16_t threadID __if_cxx(= thread_id_get_fast()));

/**
 * Convenience wrapper to acquire a lock with an unlimited timeout.  See
 * `flaglock_trylock` for more details.
 */
__always_inline static inline void flaglock_lock(FlagLockState *lock)
{
	Timeout t{UnlimitedTimeout};
	flaglock_trylock(&t, lock);
}

/**
 * Convenience wrapper to acquire a lock with an unlimited timeout.  See
 * `flaglock_priority_inheriting_trylock` for more details.
 */
__always_inline static inline void
flaglock_priority_inheriting_lock(FlagLockState *lock, uint16_t threadID)
{
	Timeout t{UnlimitedTimeout};
	flaglock_priority_inheriting_trylock(&t, lock, threadID);
}

/**
 * Unlock a flag lock.  This can be called with either form of flag lock.
 */
void __cheri_libcall flaglock_unlock(FlagLockState *lock);

/**
 * Try to acquire a recursive mutex.   This is a priority-inheriting mutex that
 * can be acquired multiple times by the same thread.
 *
 * A higher-priority thread that calls this function while a lock is held will
 * lend its priority to the thread that successfully called this function until
 * that thread either releases the lock with `flaglock_unlock` or the timeout
 * expires.
 *
 * Returns 0 on success, -ETIMEDOUT if the timeout expired, or -EINVAL if the
 * arguments are invalid.  Can also return -EOVERFLOW if the lock depth would
 * overflow the depth counter.
 */
int __cheri_libcall
recursivemutex_trylock(Timeout             *timeout,
                       RecursiveMutexState *lock,
                       uint16_t threadID    __if_cxx(= thread_id_get_fast()));

/**
 * Unlock a recursive mutex.  Note: This does not check that the current thread
 * holds the lock.  An error-checking mutex can be implemented by wrapping this
 * in a check that the `owner` field matches the current thread ID.
 *
 * Returns 0 on success.  Succeeds unconditionally (future versions may return
 * non-zero on error).
 */
int __cheri_libcall recursivemutex_unlock(RecursiveMutexState *mutex);

/**
 * Acquire a ticket lock.  Ticket locks, by design, cannot support a try-lock
 * operation and so will block forever until the lock is acquired.
 */
void __cheri_libcall ticketlock_lock(TicketLockState *lock);

/**
 * Release a ticket lock.
 */
void __cheri_libcall ticketlock_unlock(TicketLockState *lock);

/**
 * Semaphore get operation, decrements the semaphore count.  Returns 0 on
 * success, -ETIMEDOUT if the timeout expired.  Can also return -EINVAL if the
 * arguments are invalid.
 */
int __cheri_libcall semaphore_get(Timeout                *timeout,
                                  CountingSemaphoreState *semaphore);
/**
 * Semaphore put operation.  Returns 0 on success, -EINVAL if this would push
 * the semaphore count above the maximum.
 */
int __cheri_libcall semaphore_put(CountingSemaphoreState *semaphore);

__END_DECLS

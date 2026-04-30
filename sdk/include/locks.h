#pragma once
#include <cdefs.h>
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
	struct FlagLockState lock;
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

/**
 * State for a barrier.
 *
 * A barrier is a primitive that allows a set of threads to rendezvous.  Each
 * thread arrives at the barrier and blocks.  All threads can proceed once
 * every thread is there.  Barriers must be initialised with the number of
 * threads that can reach them.
 */
struct BarrierState
{
	/**
	 * The number of threads that have to rendezvous with this barrier to allow
	 * waiters to wake.
	 */
	_Atomic(uint32_t) remaining;
};

/**
 * Condition variable state.
 */
struct ConditionVariableState
{
	/**
	 * Sequence counter used to wait and notify waiters.
	 */
	_Atomic(uint32_t) sequenceCounter;
};

/**
 * State-machine states for run-once functions.
 */
enum OnceStateMachineStates : uint32_t
{
	/**
	 * The function has not run yet.
	 */
	OnceStateNotRun = 0,
	/**
	 * The function has not yet run, but is running.
	 */
	OnceStateRunning = 1,
	/**
	 * The function has run to completion.
	 */
	OnceStateRun = 2
};

/**
 * Synchronisation state for run-once functions.
 *
 * This must be initialised to 0 (`OnceStateNotRun`) in C code.  This happens
 * automatically for globals and almost all correct uses of this type are in
 * globals.
 */
struct OnceState
{
	_Atomic(enum OnceStateMachineStates)
	  state __if_cxx(= OnceStateMachineStates::OnceStateNotRun);
};

__BEGIN_DECLS

/**
 * Try to lock a flag lock.  This is the non-priority-inheriting version,
 * sometimes called a binary semaphore on other platforms.
 *
 * Returns 0 on success, -ETIMEDOUT if the timeout expired, -EINVAL if the
 * arguments are invalid, or -ENOENT if the lock is set in destruction mode.
 */
int __cheri_libcall flaglock_trylock(Timeout              *timeout,
                                     struct FlagLockState *lock);

/**
 * Try to lock a flag lock.  This is the priority-inheriting version.  Some
 * other platforms refer to this as a priority-inheriting mutex or simply a
 * mutex.
 *
 * A higher-priority thread that calls this function while a lock is held will
 * lend its priority to the thread that successfully called this function until
 * that thread either releases the lock with `flaglock_unlock` or the timeout
 * expires.
 *
 * Returns 0 on success, -ETIMEDOUT if the timeout expired, -EINVAL if the
 * arguments are invalid, or -ENOENT if the lock is set in destruction mode.
 *
 * Note: if the object is deallocated while trying to acquire the lock, then
 * this will fault.  In many cases, this is called at a compartment boundary
 * and so this is fine.  If it is not acceptable, use `heap_claim_ephemeral` to
 * ensure that the object remains live until after the call.
 */
int __cheri_libcall
flaglock_priority_inheriting_trylock(Timeout              *timeout,
                                     struct FlagLockState *lock);

/**
 * Convenience wrapper to acquire a lock with an unlimited timeout.  See
 * `flaglock_trylock` for more details.
 */
__always_inline static inline void flaglock_lock(struct FlagLockState *lock)
{
	Timeout t = {0, UnlimitedTimeout};
	flaglock_trylock(&t, lock);
}

/**
 * Convenience wrapper to acquire a lock with an unlimited timeout.  See
 * `flaglock_priority_inheriting_trylock` for more details.
 */
__always_inline static inline void
flaglock_priority_inheriting_lock(struct FlagLockState *lock)
{
	Timeout t = {0, UnlimitedTimeout};
	flaglock_priority_inheriting_trylock(&t, lock);
}

/**
 * Unlock a flag lock.  This can be called with either form of flag lock.
 */
void __cheri_libcall flaglock_unlock(struct FlagLockState *lock);

/**
 * Set a flag lock in destruction mode.
 *
 * Locks in destruction mode cannot be acquired by other threads. Any threads
 * currently attempting to acquire the lock will wake and fail to acquire the
 * lock.  This should be called before deallocating an object that contains a
 * lock.
 *
 * Note that callers do not need to hold the lock; the ability to upgrade for
 * destruction without holding the lock is useful for cleaning up from the
 * error handler.
 */
void __cheri_libcall
flaglock_upgrade_for_destruction(struct FlagLockState *lock);

/**
 * Return the thread ID of the owner of the lock.
 *
 * This is only available for priority inherited locks, as this is the only
 * case where we store the thread ID of the owner.
 *
 * The return value is 0 if the lock is not owned or if called on a
 * non-priority inherited flag lock. The return value is undefined if called on
 * an uninitialized lock.
 *
 * This *will* race with succesful `lock` and `unlock` operations on other
 * threads, and should thus not be used to check if the lock is owned.
 *
 * The main use case for this function is in the error handler to check whether
 * or not the lock is owned by the thread on which the error handler was
 * invoked.  In this case we can call this function and compare the result with
 * `thread_id_get` to know if the current thread owns the lock.
 */
__always_inline static inline uint16_t
flaglock_priority_inheriting_get_owner_thread_id(struct FlagLockState *lock)
{
	// The lock must be held at this point for the value to be stable so do
	// a non-atomic read (simply &ing the lock word would result in a
	// libcall for the atomic operation).
	return ((*(uint32_t *)&(lock->lockWord)) & 0x0000ffff);
}

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
int __cheri_libcall recursivemutex_trylock(Timeout                    *timeout,
                                           struct RecursiveMutexState *lock);

/**
 * Unlock a recursive mutex.  Note: This does not check that the current thread
 * holds the lock.  An error-checking mutex can be implemented by wrapping this
 * in a check that the `owner` field matches the current thread ID.
 *
 * Returns 0 on success.  Succeeds unconditionally (future versions may return
 * non-zero on error).
 */
int __cheri_libcall recursivemutex_unlock(struct RecursiveMutexState *mutex);

/**
 * Acquire a ticket lock.  Ticket locks, by design, cannot support a try-lock
 * operation and so will block forever until the lock is acquired.
 */
void __cheri_libcall ticketlock_lock(struct TicketLockState *lock);

/**
 * Release a ticket lock.
 */
void __cheri_libcall ticketlock_unlock(struct TicketLockState *lock);

/**
 * Semaphore get operation, decrements the semaphore count.  Returns 0 on
 * success, -ETIMEDOUT if the timeout expired.  Can also return -EINVAL if the
 * arguments are invalid.
 */
int __cheri_libcall semaphore_get(Timeout                       *timeout,
                                  struct CountingSemaphoreState *semaphore);
/**
 * Semaphore put operation.  Returns 0 on success, -EINVAL if this would push
 * the semaphore count above the maximum.
 */
int __cheri_libcall semaphore_put(struct CountingSemaphoreState *semaphore);

/**
 * Wait on a barrier.  Returns 0 or 1 on success, or `-ETIMEDOUT` on failure.
 *
 *  - 0 indicates that this was the last thread to reach the barrier.
 *  - 1 indicates that this was a thread that blocked on the barrier and has now
 *    woken.
 *  - `-ETIMEDOUT` indicates that not all threads reached the barrier prior to
 * the timeout.
 *
 * Note that barriers that are reached via
 */
int __cheriot_libcall barrier_timed_wait(Timeout             *timeout,
                                         struct BarrierState *barrier);

/**
 * Helper that calls `barrier_timed_wait` with an unlimited timeout.
 */
static int barrier_wait(struct BarrierState *barrier)
{
	Timeout t = {0, UnlimitedTimeout};
	return barrier_timed_wait(&t, barrier);
}

/**
 * Signal the condition variable and wake up to the number of waiters specified
 * by the second argument.
 *
 * Returns 0 on success.  Currently has no failure conditions.
 */
int __cheriot_libcall
condition_variable_notify(struct ConditionVariableState *conditionVariable,
                          uint32_t                       waiters);

/**
 * Wait on a condition variable.  This operation releases a mutex and
 * atomically waits for the condition variable to be signalled.
 *
 * The mutex is provided via the `mutex` parameter and the operations to lock
 * and unlock it are passed via the `mutexLock` and `mutexUnlock` parameters.
 * These must return 0 on success.  Any non-zero result will be propagated to
 * the caller.  The `mutexUnlock` function does not take a timeout and so
 * should not return `-ETIMEDOUT`.
 *
 * Returns 0 on success, and either `-ETIMEDOUT` or an error forwarded from one
 * of the callbacks on failure.
 *
 * Note that, unlike the POSIX equivalent, this does *not* reacquire the
 * condition variable if this operation times out.  This can be wrapped to
 * produce POSIX-compatible behaviour by calling the mutex-wait function with
 * an unlimited timeout if necessary, however the POSIX behaviour cannot be
 * wrapped to provide a variant that does not expire past deadlines.
 *
 * This implementation mirrors the algorithm used in Bionic (Android) and has
 * the bug that, if another thread calls `condition_variable_notify` *exactly*
 * 2^32 times in between this implementation releasing the lock and sleeping,
 * and then no thread subsequently signals the condition variable, then the
 * thread calling `condition_variable_wait` will sleep forever.
 */
int __cheriot_libcall
condition_variable_wait(Timeout                       *t,
                        struct ConditionVariableState *conditionVariable,
                        void                          *mutex,
                        int (*mutexLock)(Timeout *, void *),
                        int (*mutexUnlock)(void *));

int __cheriot_libcall run_once_slow_path(struct OnceState *state,
                                         void (*callback)(void));

__always_inline
static inline int run_once(struct OnceState *state, void (*callback)(void))
{
	// Fast path, if intitialisation has run then we can elide the call with a single load and compare.
	if (atomic_load_explicit(
	      &state->state, __if_cxx(std::) memory_order_relaxed) == OnceStateRun)
	{
		return 0;
	}
	// If this either hasn't run or is running, hit the slow path.
	return run_once_slow_path(state, callback);
}

__END_DECLS

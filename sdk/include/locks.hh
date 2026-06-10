#pragma once

#include <cdefs.h>
#include <cheriot-atomic.hh>
#include <debug.hh>
#include <errno.h>
#include <futex.h>
#include <locks.h>
#include <thread.h>

/**
 * \file
 * \brief C++ wrappers for synchronisation primitives.
 */

static constexpr bool DebugLocks =
#ifdef DEBUG_LOCKS
  DEBUG_LOCKS
#else
  false
#endif
  ;
using LockDebug = ConditionalDebug<DebugLocks, "Locking">;

__clang_ignored_warning_push("-Watomic-alignment");

/**
 * A simple flag log, wrapping an atomic word used with the `futex` calls.
 * Threads blocked on this will be woken in priority order. If
 * `IsPriorityInherited` is set, priority is inherited by waiters to avoid
 * priority inversion issues.
 *
 * The lock word that this wraps is directly accessibly by any malicious
 * compartment that has a reference to this thread.  If this is a security
 * concern then you may have other problems: a malicious compartment with
 * access to a mutex's interface (irrespective of the underlying
 * implementation) can cause deadlock by spuriously acquiring a lock or cause
 * data corruption via races by spuriously releasing it.  Anything that
 * requires mutual exclusion in the presence of mutual distrust should
 * consider an using a lock manager compartment with an API that returns a
 * single-use capability to unlock on any lock call.
 *
 * Flag locks are not recursive; if needed, use a `RecursiveMutex` instead.
 */
template<bool IsPriorityInherited>
class FlagLockGeneric
{
	FlagLockState state;

	/**
	 * Attempt to acquire the lock, blocking until a timeout specified by the
	 * `timeout` parameter has expired.
	 *
	 * Returns an errno value.
	 */
	__always_inline int try_lock_internal(Timeout *timeout)
	{
		if constexpr (IsPriorityInherited)
		{
			return flaglock_priority_inheriting_trylock(timeout, &state);
		}
		else
		{
			return flaglock_trylock(timeout, &state);
		}
	}

	public:
	/**
	 * Attempt to acquire the lock, blocking until a timeout specified by the
	 * `timeout` parameter has expired.
	 *
	 * Returns `true` if and only if the lock transitioned from being unheld to
	 * held by the current thread.
	 */
	__always_inline bool try_lock(Timeout *timeout)
	{
		return try_lock_internal(timeout) == 0;
	}

	/**
	 * Try to acquire the lock, do not block.
	 *
	 * Returns `true` if and only if the lock transitioned from being unheld to
	 * held by the current thread.
	 */
	__always_inline bool try_lock()
	{
		Timeout t{0};
		return try_lock(&t);
	}

	/**
	 * Acquire the lock, potentially blocking forever.
	 *
	 * This function returns `void` despite that this is a bad idea.
	 * Because the C++ standard library has `void`-returning `::lock()` methods,
	 * templated callers would not check a return value,
	 * making it unsafe to return in the event of failed lock acquisition.
	 * Failures instead manifest as tripping a `Debug::Invariant`,
	 * causing this function not to return (and yet not block forever).
	 * Thus, it is not safe with locks that may be put into "destrucion mode"
	 * (see `flaglock_upgrade_for_destruction`).
	 * Attempting to recursively `lock` a held priority inheriting flag lock
	 * (acquired by either `lock` or `try_lock`) will trip the same Invariant.
	 * (Recall that flag locks explicitly do not support recursive acquisition;
	 * the failure mode is, however, not one of blocking forever.)
	 * In general, defensive code should prefer `try_lock`.
	 */
	__always_inline void lock()
	{
		Timeout t{UnlimitedTimeout};
		auto    res = try_lock_internal(&t);
		LockDebug::Invariant(res == 0, "FlagLock lock() failed: {}", res);
	}

	/**
	 * Release the lock.
	 *
	 * Note: This does not check that the lock is owned by the calling thread.
	 */
	__always_inline void unlock()
	{
		flaglock_unlock(&state);
	}

	/**
	 * Set the lock in destruction mode. See the documentation of
	 * `flaglock_upgrade_for_destruction` for more information.
	 */
	__always_inline void upgrade_for_destruction()
	{
		flaglock_upgrade_for_destruction(&state);
	}

	/**
	 * Return the thread ID of the owner of the lock.
	 *
	 * This is only available for priority inherited locks, as this is the
	 * only case where we store the thread ID of the owner. See the
	 * documentation of `flaglock_priority_inheriting_get_owner_thread_id`
	 * for more information.
	 */
	__always_inline uint16_t get_owner_thread_id()
	    requires(IsPriorityInherited)
	{
		return flaglock_priority_inheriting_get_owner_thread_id(&state);
	}
};

/**
 * Priority-inheriting recursive mutex.  This can be acquired multiple times
 * from the same thread.
 */
class RecursiveMutex
{
	/// State for the underling recursive mutex.
	RecursiveMutexState state;

	public:
	/**
	 * Attempt to acquire the lock, blocking until a timeout specified by the
	 * `timeout` parameter has expired.
	 */
	__always_inline bool try_lock(Timeout *timeout)
	{
		return recursivemutex_trylock(timeout, &state) == 0;
	}

	/**
	 * Try to acquire the lock, do not block.
	 */
	__always_inline bool try_lock()
	{
		Timeout t{0};
		return try_lock(&t);
	}

	/**
	 * Acquire the lock, potentially blocking forever.
	 */
	__always_inline void lock()
	{
		Timeout t{UnlimitedTimeout};
		try_lock(&t);
	}

	/**
	 * Release the lock.
	 *
	 * Note: This does not check that the lock is owned by the calling thread.
	 */
	__always_inline void unlock()
	{
		recursivemutex_unlock(&state);
	}

	/**
	 * Set the lock in destruction mode. See the documentation of
	 * `flaglock_upgrade_for_destruction` for more information.
	 */
	__always_inline void upgrade_for_destruction()
	{
		flaglock_upgrade_for_destruction(&state.lock);
	}
};

/**
 * A simple ticket lock.
 *
 * A ticket lock ensures that threads that arrive are serviced in order,
 * without regard for priorities.  It has no mechanism for tracking tickets
 * that are discarded and so does not implement a `try_lock` API.
 */
class TicketLock
{
	TicketLockState state;

	public:
	/**
	 * Acquire the lock.
	 */
	__always_inline void lock()
	{
		ticketlock_lock(&state);
	}

	/**
	 * Release the lock.
	 *
	 * Note: This does not check that the lock is owned by the calling thread.
	 */
	__always_inline void unlock()
	{
		ticketlock_unlock(&state);
	}
};

/**
 * Class that implements the locking concept but does not perform locking.
 * This is intended to be used with templated data structures that support
 * locking, for instantiations that do not require locking.
 */
class NoLock
{
	public:
	/**
	 * Attempt to acquire the lock with a timeout.  Always succeeds.
	 */
	bool try_lock(Timeout *timeout)
	{
		return true;
	}

	/**
	 * Try to acquire the lock, do not block.  Always succeeds.
	 */
	bool try_lock()
	{
		return true;
	}

	/**
	 * Acquire the lock.  Always succeeds
	 */
	void lock() {}

	/**
	 * Release the lock.  Does nothing.
	 */
	void unlock() {}
};

using FlagLock                  = FlagLockGeneric<false>;
using FlagLockPriorityInherited = FlagLockGeneric<true>;

template<typename T>
concept Lockable = requires(T l) {
	{ l.lock() };
	{ l.unlock() };
};

template<typename T>
concept TryLockable = Lockable<T> && requires(T l, Timeout *t) {
	{ l.try_lock(t) } -> std::same_as<bool>;
};

static_assert(TryLockable<NoLock>);
static_assert(TryLockable<FlagLock>);
static_assert(TryLockable<FlagLockPriorityInherited>);
static_assert(Lockable<TicketLock>);

/**
 * A simple RAII type that owns a lock.
 */
template<typename Lock>
class LockGuard
{
	/// A reference to the managed lock
	Lock *wrappedLock;

	/// Flag indicating whether the lock is owned.
	bool isOwned;

	public:
	/**
	 * Constructor, acquires the lock with unbounded wait.
	 *
	 * The lock to be wrapped must not be held by the calling thread on entry.
	 * Further, for this constructor to be safe to invoke,
	 * the `Lock` type must have an unfailable `lock` member;
	 * for example, this precludes the use of `FlagLock`-s
	 * that may be placed in "destruction mode".
	 *
	 * In general, prefer the `LockGuard` constructor that takes a `Timeout`.
	 */
	[[nodiscard]] explicit LockGuard(Lock &lock)
	  : wrappedLock(&lock), isOwned(true)
	{
		wrappedLock->lock();
	}

	/**
	 * Constructor, attempts to acquire the lock with a timeout.
	 *
	 * The lock to be wrapped must not be held by the calling thread on entry.
	 */
	[[nodiscard]] explicit LockGuard(Lock &lock, Timeout *timeout)
	    requires(TryLockable<Lock>)
	  : wrappedLock(&lock), isOwned(false)
	{
		try_lock(timeout);
	}

	/// Move constructor, transfers ownership of the lock.
	[[nodiscard]] explicit LockGuard(LockGuard &&guard)
	  : wrappedLock(guard.wrappedLock), isOwned(guard.isOwned)
	{
		guard.wrappedLock = nullptr;
		guard.isOwned     = false;
	}

	/**
	 * Explicitly lock the wrapped lock.
	 *
	 * The wrapped lock must not be held by the calling thread on entry.
	 * For this method to be safe to call,
	 * the `Lock` type must have an unfailable `lock` member;
	 * for example, this precludes the use of `FlagLock`-s
	 * that may be placed in "destruction mode".
	 * In general, prefer `try_lock`.
	 */
	void lock()
	{
		LockDebug::Assert(!isOwned, "Trying to lock an already-locked lock");
		wrappedLock->lock();
		isOwned = true;
	}

	/**
	 * Explicitly lock the wrapped lock. Must be called with the lock locked by
	 * this wrapper.
	 */
	void unlock()
	{
		LockDebug::Assert(isOwned, "Trying to unlock an unlocked lock");
		wrappedLock->unlock();
		isOwned = false;
	}

	/**
	 * Unwrap the lock without unlocking it. This is useful to call before
	 * destroying a lock.
	 */
	void release()
	{
		wrappedLock = nullptr;
		isOwned     = false;
	}

	/**
	 * If the wrapped lock type supports locking with a timeout,
	 * try to lock it with the specified timeout.
	 *
	 * The wrapped lock must not be held by the calling thread on entry.
	 * Returns true if the lock has been acquired, false otherwise.
	 */
	bool try_lock(Timeout *timeout)
	    requires(TryLockable<Lock>)
	{
		LockDebug::Assert(!isOwned, "Trying to lock an already-locked lock");
		isOwned = wrappedLock->try_lock(timeout);
		return isOwned;
	}

	/// Destructor, releases the lock.
	~LockGuard()
	{
		if (isOwned)
		{
			wrappedLock->unlock();
		}
	}

	/**
	 * Conversion to bool.  Returns true if this guard owns the lock, false
	 * otherwise.  This allows lock guards to be used with a timeout in
	 * conditional blocks, such as:
	 *
	 * ```
	 * if (LockGuard g{lock, timeout})
	 * {
	 *    // Run this code if we acquired the lock, releasing the lock at the
	 * end.
	 * }
	 * else
	 * {
	 *    // Run this code if we did not acquire the lock.
	 * }
	 * ```
	 */
	operator bool()
	{
		return isOwned;
	}

	/**
	 * Drop and reacquire the lock around a yield
	 */
	int yield(Timeout *t, uint32_t ticks = 1)
	{
		unlock();

		Timeout smallSleep{ticks};
		if (int sleepRes = thread_sleep(&smallSleep); sleepRes < 0)
		{
			return sleepRes;
		}

		t->elapse(smallSleep.elapsed);

		return try_lock(t);
	}
};

/**
 * Condition variable C++ wrapper.
 */
class ConditionVariable
{
	/**
	 * The state for the underlying condition variable.
	 */
	ConditionVariableState state;

	public:
	/**
	 * Wake one waiter.  Propagates errors from `condition_variable_notify`.
	 */
	int signal()
	{
		return condition_variable_notify(&state, 1);
	}

	/**
	 * Wake all waiters.  Propagates errors from `condition_variable_notify`.
	 */
	int broadcast()
	{
		return condition_variable_notify(&state,
		                                 std::numeric_limits<uint32_t>::max());
	}

	/**
	 * Atomically release `mutex`, wait until the condition variable is
	 * signalled, and reacquire the mutex.  This will return 0 if the sequence
	 * completes correctly, or `-ETIMEDOUT` if waiting or reacquiring the lock
	 * failed due to a timeout.
	 */
	template<TryLockable Mutex>
	int wait(Timeout *t, Mutex &mutex)
	{
		auto lock = [](Timeout *t, void *m) {
			return static_cast<Mutex *>(m)->try_lock(t) ? 0 : -ETIMEDOUT;
		};
		auto unlock = [](void *m) {
			static_cast<Mutex *>(m)->unlock();
			return 0;
		};
		return condition_variable_wait(t, &state, &mutex, lock, unlock);
	}

	/**
	 * Atomically release `mutex`, wait until the condition variable is
	 * signalled, and reacquire the mutex.  This variant uses locks with no
	 * timeout and so may block for longer than the specified timeout while
	 * reacquiring the lock.  It will not attempt to reacquire the lock if the
	 * timeout happens before the condition variable is signalled, so can still
	 * fail in the same ways as the normal overload.
	 */
	template<Lockable Mutex>
	int wait(Timeout *t, Mutex &mutex)
	    requires(!TryLockable<Mutex>)
	{
		auto lock = [](Timeout *t, void *m) {
			static_cast<Mutex *>(m)->lock();
			return 0;
		};
		auto unlock = [](void *m) {
			static_cast<Mutex *>(m)->unlock();
			return 0;
		};
		return condition_variable_wait(t, &state, &mutex, lock, unlock);
	}
};

__clang_ignored_warning_pop();

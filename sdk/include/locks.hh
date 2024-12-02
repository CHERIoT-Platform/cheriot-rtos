#pragma once

#include <cdefs.h>
#include <cheriot-atomic.hh>
#include <debug.hh>
#include <errno.h>
#include <futex.h>
#include <locks.h>
#include <thread.h>

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
 */
template<bool IsPriorityInherited>
class FlagLockGeneric
{
	FlagLockState state;

	public:
	/**
	 * Attempt to acquire the lock, blocking until a timeout specified by the
	 * `timeout` parameter has expired.
	 */
	__always_inline bool try_lock(Timeout *timeout)
	{
		if constexpr (IsPriorityInherited)
		{
			return flaglock_priority_inheriting_trylock(timeout, &state) == 0;
		}
		else
		{
			return flaglock_trylock(timeout, &state) == 0;
		}
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
	__always_inline uint16_t get_owner_thread_id() requires(IsPriorityInherited)
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
concept Lockable = requires(T l)
{
	{l.lock()};
	{l.unlock()};
};

template<typename T>
concept TryLockable = Lockable<T> && requires(T l, Timeout *t)
{
	{
		l.try_lock(t)
		} -> std::same_as<bool>;
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
	/// Constructor, acquires the lock.
	[[nodiscard]] explicit LockGuard(Lock &lock)
	  : wrappedLock(&lock), isOwned(true)
	{
		wrappedLock->lock();
	}

	/// Constructor, attempts to acquire the lock with a timeout.
	[[nodiscard]] explicit LockGuard(Lock &lock, Timeout *timeout) requires(
	  TryLockable<Lock>)
	  : wrappedLock(&lock), isOwned(false)
	{
		try_lock(timeout);
	}

	/// Constructor, attempts to acquire the lock with a timeout, in steps
	/// of a given duration, checking the `condition` function at every
	/// iteration. If `condition` returns `true`, stop trying to acquire
	/// the lock.
	[[nodiscard]] explicit LockGuard(Lock    &lock,
	                                 Timeout *timeout,
	                                 Ticks    step,
	                                 auto condition) requires(TryLockable<Lock>)
	  : wrappedLock(&lock), isOwned(false)
	{
		do
		{
			Timeout t{std::min(step, timeout->remaining)};
			try_lock(&t);
			if (!t.elapsed)
			{
				// We failed to lock, likely because the lock
				// is set for destruction.
				break;
			}
			timeout->elapse(t.elapsed);
		} while (!isOwned && timeout->may_block() && !condition());
	}

	/// Constructor, attempts to acquire the lock with a timeout, in steps
	/// of a given duration.
	[[nodiscard]] explicit LockGuard(Lock    &lock,
	                                 Timeout *timeout,
	                                 Ticks    step) requires(TryLockable<Lock>)
	  : LockGuard(lock, timeout, step, []() { return false; })
	{
	}

	/// Move constructor, transfers ownership of the lock.
	[[nodiscard]] explicit LockGuard(LockGuard &&guard)
	  : wrappedLock(guard.wrappedLock), isOwned(guard.isOwned)
	{
		guard.wrappedLock = nullptr;
		guard.isOwned     = false;
	}

	/**
	 * Explicitly lock the wrapped lock. Must be called with the lock unlocked.
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
	 * If the underlying lock type supports locking with a timeout, try to lock
	 * it with the specified timeout. This must be called with the lock
	 * unlocked.  Returns true if the lock has been acquired, false otherwise.
	 */
	bool try_lock(Timeout *timeout) requires(TryLockable<Lock>)
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
};

__clang_ignored_warning_pop();

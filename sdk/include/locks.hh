#pragma once

#include <cdefs.h>
#include <debug.hh>
#include <futex.h>
#include <semaphore.h>

__clang_ignored_warning_push("-Watomic-alignment")

  static constexpr bool DebugLocks =
#ifdef DEBUG_LOCKS
    DEBUG_LOCKS
#else
    false
#endif
  ;
using LockDebug = ConditionalDebug<DebugLocks, "Locking">;

/**
 * A simple flag log, wrapping an atomic word used with the `futex` calls.
 * Threads blocked on this will be woken in priority order but this does not
 * propagate priority and so can lead to priority inversion if a low-priority
 * thread is attempting to acquire a flag lock to perform an operation on
 * behalf of a high priority thread.
 *
 * The lock word that this wraps is directly accessibly by any malicious
 * compartment that has a reference to this thread.  If this is a security
 * concern then you may have other problems: a malicious compartment with
 * access to a mutex's interface (irrespective of the underlying
 * implementation) can cause deadlock by spuriously acquiring a lock or cause
 * data corruption via races by spuriously releasing it.  Anything that
 * requires mutual exclusion in the presence of mutual distrust should consider
 * an using a lock manager compartment with an API that returns a single-use
 * capability to unlock on any lock call.
 */
class FlagLock
{
	/**
	 * States used in the futex word.
	 */
	enum Flag : uint32_t
	{
		/// The lock is not held.
		Unlocked,
		/// The lock is held.
		Locked,
		/// The lock is held and one or more threads are waiting on it.
		LockedWithWaiters
	};

	/// The lock word.
	_Atomic(Flag) flag = Flag::Unlocked;

	/**
	 * Returns the lock word with the type that the `futex_*` family of calls
	 * expect.
	 */
	uint32_t *futex_word()
	{
		return reinterpret_cast<uint32_t *>(&flag);
	}

	/**
	 * Convenience wrapper around the atomic builtins to operate on the flag
	 * words.
	 */
	__always_inline bool compare_and_swap_flag(Flag &expected, Flag desired)
	{
		return __c11_atomic_compare_exchange_strong(
		  &flag, &expected, desired, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	}

	public:
	/**
	 * Attempt to acquire the lock, blocking until a timeout specified by the
	 * `timeout` parameter has expired.
	 */
	bool try_lock(Timeout *timeout)
	{
		Flag old = Flag::Unlocked;
		if (compare_and_swap_flag(old, Flag::Locked))
		{
			return true;
		}
		while (timeout->remaining > 0)
		{
			// If there are already waiters, don't bother with the atomic call.
			if (old != Flag::LockedWithWaiters)
			{
				LockDebug::Assert(
				  old == Flag::Locked, "Unexpected flag value: {}", old);
				compare_and_swap_flag(old, Flag::LockedWithWaiters);
			}
			if (old != Flag::Unlocked)
			{
				LockDebug::log("hitting slow path wait for {}", &flag);
				futex_timed_wait(futex_word(), old, timeout);
			}
			old = Flag::Unlocked;
			// Try to acquire, acquire with waiters so that we don't lose wakes
			// if we win a race.
			if (compare_and_swap_flag(old, Flag::LockedWithWaiters))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Try to acquire the lock, do not block.
	 */
	bool try_lock()
	{
		Timeout t{0};
		return try_lock(&t);
	}

	/**
	 * Acquire the lock, potentially blocking forever.
	 */
	void lock()
	{
		Timeout t{UnlimitedTimeout};
		try_lock(&t);
	}

	/**
	 * Release the lock.
	 *
	 * Note: This does not check that the lock is owned by the calling thread.
	 */
	void unlock()
	{
		Flag old =
		  __c11_atomic_exchange(&flag, Flag::Unlocked, __ATOMIC_SEQ_CST);
		LockDebug::Assert(old != Flag::Unlocked, "Double-unlocking {}", &flag);
		// If there are waiters, wake one.
		if (old == Flag::LockedWithWaiters)
		{
			LockDebug::log("hitting slow path wake for {}", &flag);
			futex_wake(futex_word(), std::numeric_limits<uint32_t>::max());
		}
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
	/**
	 * The value of the current ticket being served.
	 */
	_Atomic(uint32_t) current;

	/**
	 * The next ticket that a caller can take.
	 */
	_Atomic(uint32_t) next;

	/**
	 * Returns the lock word with the type that the `futex_*` family of calls
	 * expect.
	 */
	uint32_t *futex_word()
	{
		return reinterpret_cast<uint32_t *>(&current);
	}

	public:
	/**
	 * Acquire the lock.
	 */
	void lock()
	{
		uint32_t ticket = next++;
		do
		{
			uint32_t currentSnapshot = current;
			if (currentSnapshot == ticket)
			{
				return;
			}
			futex_wait(futex_word(), currentSnapshot);
		} while (true);
	}

	/**
	 * Release the lock.
	 *
	 * Note: This does not check that the lock is owned by the calling thread.
	 */
	void unlock()
	{
		uint32_t currentSnapshot = current++;
		if (next > currentSnapshot)
		{
			futex_wake(futex_word(), std::numeric_limits<uint32_t>::max());
		}
	}
};

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

static_assert(TryLockable<FlagLock>);
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
};
__clang_ignored_warning_pop()

#pragma once

#include <cdefs.h>
#include <cheriot-atomic.hh>
#include <debug.hh>
#include <errno.h>
#include <futex.h>
#include <thread.h>

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
template<bool IsPriorityInherited>
class FlagLockGeneric
{
	/**
	 * States used in the futex word.
	 */
	enum Flag : uint32_t
	{
		/// The lock is not held.
		Unlocked = 0,
		/// The lock is held.
		Locked = 1 << 16,
		/// The lock is held and one or more threads are waiting on it.
		LockedWithWaiters = 1 << 17
	};

	/// The lock word.
	cheriot::atomic<uint32_t> flag = Flag::Unlocked;

	/**
	 * Returns the bits to use as the thread ID.  For priority inherited locks,
	 * this is the real current thread ID, otherwise it is simply 0.
	 */
	__always_inline uint16_t thread_id()
	{
		if constexpr (IsPriorityInherited)
		{
			return thread_id_get_fast();
		}
		return 0;
	}

	public:
	/**
	 * Attempt to acquire the lock, blocking until a timeout specified by the
	 * `timeout` parameter has expired.
	 */
	bool try_lock(Timeout *timeout)
	{
		uint32_t old        = Flag::Unlocked;
		auto     threadBits = thread_id();
		uint32_t desired    = Flag::Locked | threadBits;
		if (flag.compare_exchange_strong(old, desired))
		{
			return true;
		}
		// Next time, try to acquire, acquire with waiters so that we don't
		// lose wakes if we win a race.
		desired = Flag::LockedWithWaiters | threadBits;
		while (timeout->remaining > 0)
		{
			// If there are not already waiters, set the waiters flag.
			if ((old & Flag::LockedWithWaiters) == 0)
			{
				LockDebug::Assert((old & 0xffff0000) == Flag::Locked,
				                  "Unexpected flag value: {}",
				                  old);
				uint32_t addedWaiters = Flag::LockedWithWaiters;
				if constexpr (IsPriorityInherited)
				{
					addedWaiters |= (old & 0xffff);
				}
				flag.compare_exchange_strong(old, addedWaiters);
			}
			if (old != Flag::Unlocked)
			{
				LockDebug::log("Hitting slow path wait for {}", &flag);
				FutexWaitFlags flags =
				  IsPriorityInherited ? FutexPriorityInheritance : FutexNone;
				if (flag.wait(timeout, old, flags) == -EINVAL)
				{
					return false;
				}
			}
			old = Flag::Unlocked;
			if (flag.compare_exchange_strong(old, desired))
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
		auto old = flag.exchange(Flag::Unlocked);
		LockDebug::Assert(old != Flag::Unlocked, "Double-unlocking {}", &flag);
		// If there are waiters, wake them all up.
		if ((old & Flag::LockedWithWaiters) != 0)
		{
			LockDebug::log("hitting slow path wake for {}", &flag);
			flag.notify_all();
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
	cheriot::atomic<uint32_t> current;

	/**
	 * The next ticket that a caller can take.
	 */
	cheriot::atomic<uint32_t> next;

	public:
	/**
	 * Acquire the lock.
	 */
	void lock()
	{
		uint32_t ticket = next++;
		LockDebug::log("Ticket {} issued", ticket);
		do
		{
			uint32_t currentSnapshot = current;
			if (currentSnapshot == ticket)
			{
				LockDebug::log("Ticket {} proceeding", ticket);
				return;
			}
			LockDebug::log("Ticket {} waiting for {}", ticket, currentSnapshot);
			current.wait(currentSnapshot);
		} while (true);
	}

	/**
	 * Release the lock.
	 *
	 * Note: This does not check that the lock is owned by the calling thread.
	 */
	void unlock()
	{
		uint32_t currentSnapshot = ++current;
		if (next > currentSnapshot)
		{
			current.notify_all();
		}
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

	/**
	 * Conversion to bool.  Returns true if this guard owns the lock, false
	 * otherwise.  This allows lock guards to be used with a timeout in
	 * conditional blocks, such as:
	 *
	 * ```
	 * if (LockGuard g{lock, timeout})
	 * {
	 *    // Run this code if we acquired the lock, releasing the lock at the end.
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
__clang_ignored_warning_pop()

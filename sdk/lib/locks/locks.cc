#include <atomic>
#include <debug.hh>
#include <errno.h>
#include <limits>
#include <locks.h>
#include <thread.h>

namespace
{
	constexpr bool DebugLocks =
#ifdef DEBUG_LOCKS
	  DEBUG_LOCKS
#else
	  false
#endif
	  ;
	using Debug = ConditionalDebug<DebugLocks, "Locking">;
	/**
	 * Internal implementation of a simple flag lock. See comments in
	 * locks.hh and locks.h for more details.
	 */
	struct InternalFlagLock : public FlagLockState
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
			LockedWithWaiters = 1 << 17,
			/// The lock is held and set in destruction mode.
			LockedInDestructMode = 1 << 18
		};

		public:
		/**
		 * Attempt to acquire the lock, blocking until a timeout specified by
		 * the `timeout` parameter has expired.
		 */
		int
		try_lock(Timeout *timeout, uint32_t threadID, bool isPriorityInherited)
		{
			while (true)
			{
				uint32_t old     = Flag::Unlocked;
				uint32_t desired = Flag::Locked | threadID;
				if (lockWord.compare_exchange_strong(old, desired))
				{
					return 0;
				}

				// We are not setting the LockedInDestructMode
				// bit in our expected value (`old`), so the
				// CAS will always fail if the bit is set.
				// Being locked for destruction is unusual (it
				// should happen at most once for any lock) and
				// so we don't bother to check for it unless
				// we've failed to acquire the lock.
				if ((old & Flag::LockedInDestructMode) != 0)
				{
					return -ENOENT;
				}

				if (!timeout->may_block())
				{
					return -ETIMEDOUT;
				}
				Debug::log("Hitting slow path wait for {}", &lockWord);
				// If there are not already waiters, set the waiters flag.
				if ((old & Flag::LockedWithWaiters) == 0)
				{
					Debug::Assert((old & 0xffff0000) == Flag::Locked,
					              "Unexpected flag value: {}",
					              old);
					// preserve any ThreadID
					uint32_t addedWaiters =
					  Flag::LockedWithWaiters | (old & 0xffff);
					Debug::log("Adding waiters {} => {}", old, addedWaiters);
					if (!lockWord.compare_exchange_strong(old, addedWaiters))
					{
						// Something raced with us while trying to add waiters
						// flag. Could be:
						// 1. unlock by another thread
						// 2. unlock then acquire by other thread(s)
						// 3. another thread added waiters flags
						// We could potentially handle each of these separately
						// but the simplest thing is just to start again from
						// the top.
						Debug::log("Adding waiters failed {}", old);
						continue;
					}
					// update expected value as we've just successfully written
					// it
					old = addedWaiters;
				}
				FutexWaitFlags flags =
				  isPriorityInherited ? FutexPriorityInheritance : FutexNone;
				if (int ret = lockWord.wait(timeout, old, flags); ret != 0)
				{
					// Most likely a timeout, or waiting on priority inherited
					// futex where this thread is already the owner i.e.
					// reentrancy without recursive mutex. Otherwise something
					// bad like invalid permissions.
					Debug::log("Wait failed {}", ret);
					return ret;
				}
			}
		}

		/**
		 * Return the owner.  If the lock is not held, this value should not be
		 * assumed to be stable.  This does an unordered load.
		 */
		[[nodiscard]] __always_inline uint16_t owner() const
		{
			return *reinterpret_cast<const uint16_t *>(&lockWord);
		}

		/**
		 * Release the lock.
		 *
		 * Note: This does not check that the lock is owned by the calling
		 * thread.
		 */
		void unlock()
		{
			auto old = lockWord.exchange(Flag::Unlocked);

			// Assert that the locked is not already locked, and
			// that the caller holds the lock. This is only
			// compiled-in with debugging enabled for locking.
			Debug::Assert(
			  old != Flag::Unlocked, "Double-unlocking {}", &lockWord);
			Debug::Assert(
			  (old & 0x0000ffff) == thread_id_get(),
			  "Calling thread {} does not hold the lock on {} (owner: {})",
			  thread_id_get(),
			  &lockWord,
			  old & 0x0000ffff);

			// If there are waiters, wake them all up.
			if ((old & Flag::LockedWithWaiters) != 0)
			{
				Debug::log("hitting slow path wake for {}", &lockWord);
				lockWord.notify_all();
			}
		}

		/**
		 * Set the destruction bit in the flag lock word and wake
		 * waiters. Assumes that the lock is held by the caller.
		 *
		 * Note: This does not check that the lock is owned by the
		 * calling thread.
		 */
		void upgrade_for_destruction()
		{
			Debug::log("Setting {} for destruction", &lockWord);

			// Assert that the caller holds the lock. This is only
			// compiled-in with debugging enabled for locking.
			Debug::Assert(
			  (lockWord & 0x0000ffff) == thread_id_get(),
			  "Calling thread {} does not hold the lock on {} (owner: {})",
			  thread_id_get(),
			  &lockWord,
			  lockWord & 0x0000ffff);

			// Set the destruction bit.
			lockWord |= Flag::LockedInDestructMode;

			// Wake up waiters.
			// There should not be any 'missed wake' because any
			// thread calling lock() concurrently will either:
			// - successfully CAS, pass the the -ENOENT check, reach
			//   the futex `wait` before we set the
			//   `Flag::LockedInDestructMode` bit, and be woken up
			//   by our `notify_all`;
			// - fail the -ENOENT check;
			// - miss the `Flag::LockedInDestructMode` bit, pass the
			//   -ENOENT check but fail their CAS to add the waiters
			//   bit, retry, and fail the -ENOENT check;
			// - or miss the `Flag::LockedInDestructMode` bit, pass
			//   the -ENOENT check but fail their CAS in the futex
			//   `wait` call, retry (because the futex returns 0 in
			//   this case), and fail the -ENOENT check;
			if ((lockWord & Flag::LockedWithWaiters) != 0)
			{
				Debug::log("hitting slow path wake for {}", &lockWord);
				lockWord.notify_all();
			}
		}
	};

	/**
	 * Internal implementation of a simple ticket lock. See comments in
	 * locks.hh and locks.h for more details.
	 */
	struct InternalTicketLock : public TicketLockState
	{
		/**
		 * Acquire the lock.
		 */
		void lock()
		{
			uint32_t ticket = next++;
			Debug::log("Ticket {} issued", ticket);
			do
			{
				uint32_t currentSnapshot = current;
				if (currentSnapshot == ticket)
				{
					Debug::log("Ticket {} proceeding", ticket);
					return;
				}
				Debug::log("Ticket {} waiting for {}", ticket, currentSnapshot);
				current.wait(currentSnapshot);
			} while (true);
		}

		/**
		 * Release the lock.
		 *
		 * Note: This does not check that the lock is owned by the calling
		 * thread.
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

	static_assert(sizeof(InternalFlagLock) == sizeof(FlagLockState));
	static_assert(sizeof(InternalTicketLock) == sizeof(TicketLockState));

	__clang_ignored_warning_pop()

} // namespace

int __cheri_libcall flaglock_trylock(Timeout *t, FlagLockState *lock)
{
	// The thread ID is only used for debugging, or priority inheriting
	// (which is not relevant here). To avoid a useless call to
	// thread_id_get(), pass 0 when debugging is disabled.
	uint32_t threadID = 0;
	if constexpr (DebugLocks)
	{
		threadID = thread_id_get();
	}

	return static_cast<InternalFlagLock *>(lock)->try_lock(t, threadID, false);
}
int __cheri_libcall flaglock_priority_inheriting_trylock(Timeout       *t,
                                                         FlagLockState *lock)
{
	return static_cast<InternalFlagLock *>(lock)->try_lock(
	  t, thread_id_get(), true);
}
void __cheri_libcall flaglock_unlock(FlagLockState *lock)
{
	static_cast<InternalFlagLock *>(lock)->unlock();
}

void __cheri_libcall
flaglock_upgrade_for_destruction(struct FlagLockState *lock)
{
	static_cast<InternalFlagLock *>(lock)->upgrade_for_destruction();
}

void __cheri_libcall ticketlock_lock(TicketLockState *lock)
{
	static_cast<InternalTicketLock *>(lock)->lock();
}
void __cheri_libcall ticketlock_unlock(TicketLockState *lock)
{
	static_cast<InternalTicketLock *>(lock)->unlock();
}

int recursivemutex_trylock(Timeout *timeout, RecursiveMutexState *mutex)
{
	auto              threadID = thread_id_get();
	InternalFlagLock *internalLock =
	  static_cast<InternalFlagLock *>(&mutex->lock);
	// If the lock owner is the current thread, we already own the lock.
	if (internalLock->owner() == threadID)
	{
		if (mutex->depth == std::numeric_limits<decltype(mutex->depth)>::max())
		{
			return -EOVERFLOW;
		}
		mutex->depth++;
		return 0;
	}
	// If the lock owner is not currently us, try to acquire it.
	if (internalLock->try_lock(timeout, threadID, true) == 0)
	{
		mutex->depth = 1;
		return 0;
	}
	return -ETIMEDOUT;
}

int __cheri_libcall recursivemutex_unlock(RecursiveMutexState *mutex)
{
	if ((--mutex->depth) > 0)
	{
		return 0;
	}
	static_cast<InternalFlagLock *>(&mutex->lock)->unlock();
	return 0;
}

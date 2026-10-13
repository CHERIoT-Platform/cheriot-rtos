#include "lock_debug.hh"
#include <errno.h>
#include <locks.h>

namespace
{
	/**
	 * Reader-writer lock.  This uses two futex words:
	 *
	 * The first stores state, including the number of readers and whether there
	 * are readers or writers pending.  The number of concurrent readers is
	 * stored in this word.  When a writer holds the lock, it sets this to the
	 * saturated value.
	 *
	 * The second stores an incrementing counter that is signalled whenever a
	 * writer needs to be woken.
	 *
	 * When readers arrive, they can acquire the lock if both the reader count
	 * is less than the maximum and there are no pending writers.  If this
	 * happens, they increment the lock counter.
	 *
	 * If a reader cannot acquire the lock immediately, it sets the
	 * readers-waiting flag and sleeps on the readers word.  When a reader
	 * releases the lock, it checks to see whether it was the last reader and,
	 * if so, and if there are writers waiting, it increments and notifies the
	 * writer notify word.
	 *
	 * If a writer tries to acquire the lock, it checks whether there are zero
	 * readers.  If so, it can acquire the lock directly.  If this fails, it
	 * sets the writers-waiting flag and sleeps on the writer lock.  When
	 * notified, it retries.
	 *
	 * All of this is moderately simple, except for timeouts.
	 *
	 * A *writer* timing out is the more complex case.  A writer may fail to
	 * acquire the lock if either a reader (or multiple readers) or a writer
	 * holds the lock.  If a writer holds the lock, other writers and readers
	 * may be blocked.  If a reader holds the lock, only other writers may be
	 * blocked.  For each case in turn:
	 *
	 *  - Reader holds the lock.  Reader-waiting flag clear.  In this case, the
	 *    caller *may* be the only writer, but must assume that they are not
	 *    (waiting writers are not reference counted).  On timeout, the caller
	 *    must clear the writer-waiting flag and then wake writers.  Any other
	 *    pending writers will reacquire the lock and, if they fail (which they
	 *    almost certainly will), mark the lock as having waiters and sleep
	 *    again.
	 *  - Reader holds the lock.  Reader-waiting flag set.  At least one reader
	 *    is blocked behind either us, or another higher-priority writer.
	 *    Clear the writer-waiting flag, then signal the writer futex.  We are
	 *    not trying to acquire the lock with the atomic compare-and-swap (CAS)
	 *    at this point, so we may lose a race with the last reader.  If we did
	 *    (i.e. lock count is 0), signal readers.
	 *  - Writer holds the lock.  Reader-waiting flag in either state.  Clear
	 *    the writers-waiting flag and signal writers.  This allows any pending
	 *    writers to reassert the writers-waiting flag.  If there are no
	 *    pending writers, this has no effect.  When the writer drops the lock,
	 *    it will signal reader or writer futexes as required.  It will signal
	 *    readers first.
	 *
	 * If a reader times out trying to acquire the lock then the lock must be
	 * held by a writer (in theory, it could be held by a bit over a billion
	 * readers, but that seems unlikely on an embedded system).
	 *
	 * If a reader times out trying to acquire the lock, it should clear the
	 * readers-waiting flag (via a CAS).  It may not be the only reader
	 * waiting, so must signal readers to wake and reestablish the waiting
	 * flag. If the writer has dropped the lock and there are pending writers,
	 * it must *also* wake writers.  If the writer unlocked just before we
	 * dropped the reader-waiting flag, then it's possible that we are the
	 * *only* pending reader and the next writer is backed up behind us.
	 */
	class ReadWriteLock : public ReadWriteLockState
	{
		/// Flag in the state word indicating that at least one writer is
		/// waiting
		static const uint32_t WriterWaitingFlag = (1U << 31);
		/// Flag in the state word indicating that at least one reader is
		/// waiting
		static const uint32_t ReaderWaitingFlag = (1U << 30);
		/// The low bits are used as a waiting flag
		static const uint32_t ReadersMask = ReaderWaitingFlag - 1;
		/// The write lock is held by a writer.
		static const uint32_t WriteLocked = ReadersMask;

		static_assert((ReadersMask & ReaderWaitingFlag) == 0,
		              "Reader flag overlaps readers mask!");
		static_assert((ReadersMask & WriterWaitingFlag) == 0,
		              "Writer flag overlaps readers mask!");

		/**
		 * How many readers are holding this lock?  If a writer holds the lock,
		 * returns `WriteLocked`.
		 */
		static uint32_t reader_lock_count(uint32_t state)
		{
			return state & ReadersMask;
		}

		/*
		 * We can acquire the read lock if both hold:
		 *
		 *  - There are no pending writers.
		 *  - There are not enough readers to saturate the lock.
		 */
		static bool can_acquire_read_lock(uint32_t state)
		{
			return ((state & WriterWaitingFlag) == 0) &&
			       (reader_lock_count(state) < (ReadersMask - 1));
		}

		/*
		 * We can acquire the write lock if and only if the lock is not held by
		 * anyone.
		 */
		static bool can_acquire_write_lock(uint32_t state)
		{
			return reader_lock_count(state) == 0;
		}

		/// Are there writers pending in this state word?
		static bool are_writers_pending(uint32_t state)
		{
			return state & WriterWaitingFlag;
		}

		/// Are there readers pending in this state word?
		static bool are_readers_pending(uint32_t state)
		{
			return state & ReaderWaitingFlag;
		}

		/// Does this state flag indicate the write lock is held?
		static bool is_write_locked(uint32_t state)
		{
			return reader_lock_count(state) == WriteLocked;
		}

		/// Wake any pending writers
		void wake_writers()
		{
			auto newValue = ++writerNotify;
			Debug::log(
			  "Waking writers of writer lock {} with value {}", this, newValue);
			writerNotify.notify_all();
		}

		/// Wake any pending readers
		void wake_readers()
		{
			Debug::log("Waking readers of writer lock {}", this);
			state.notify_all();
		}

		public:
		int try_read_lock(Timeout *timeout)
		{
			auto currentState = state.load(std::memory_order_relaxed);
			while (true)
			{
				// Fast path.  If there are no current or pending writers, we
				// can acquire the read lock immediately.
				while (can_acquire_read_lock(currentState))
				{
					// The check in can_acquire_read_lock ensures that
					// incrementing won't overflow into the higher bits.
					if (state.compare_exchange_strong(currentState,
					                                  currentState + 1))
					{
						return 0;
					}
				}
				Debug::log(
				  "Failed fast-path acquire of read lock {}, lock state is: {}",
				  this,
				  currentState);

				// We have left the fast path, mark this as having pending
				// readers
				if (state.compare_exchange_strong(
				      currentState, currentState | ReaderWaitingFlag))
				{
					Debug::log("Trying to acquire reader lock {}, sleeping on "
					           "futex with value {}",
					           this,
					           currentState | ReaderWaitingFlag);
					int result =
					  state.wait(timeout, currentState | ReaderWaitingFlag);

					// If we timed out, we may need to notify some other waiters
					if (result < 0)
					{
						// Get the new value of the word.  We're leaving the
						// waiting path so will need to clear the readers
						// waiting flag.
						currentState = state.load(std::memory_order_relaxed);
						do
						{
							// If someone else already cleared the
							// readers-waiting flag, they're responsible for
							// waking up everyone who might be blocked behind
							// us.  Leave them to it.
							if (!(currentState & ReaderWaitingFlag))
							{
								return result;
							}
						} while (!state.compare_exchange_strong(
						  currentState, currentState & ~ReaderWaitingFlag));

						// We have now dropped the reader-waiting flag.  Other
						// readers may be waiting, so make sure that they wake
						// up and reacquire the flag.
						wake_readers();

						// If there are writers waiting and the writer lock was
						// dropped, we need to also wake writers
						if (!is_write_locked(currentState) &&
						    are_writers_pending(currentState))
						{
							wake_writers();
						}
						return result;
					}
				}
			}
		}

		int try_write_lock(Timeout *timeout)
		{
			auto currentState = state.load(std::memory_order_relaxed);
			while (true)
			{
				// Fast path.  If the lock is not held by anyone, we
				// can acquire the write lock immediately.
				while (can_acquire_write_lock(currentState))
				{
					// Acquire the write lock by setting the lock-holders count
					// to the maximum value.
					if (state.compare_exchange_strong(
					      currentState, currentState | WriteLocked))
					{
						return 0;
					}
				}
				Debug::log("Failed fast-path acquire of write lock {}, lock "
				           "state is: {}",
				           this,
				           currentState);

				// Read the writer epoch *before* trying to do the CAS to add in
				// the writers-waiting flag.  When we sleep on this word, we
				// need to
				uint32_t writerEpoch = writerNotify.load();

				// We have left the fast path, mark this as having pending
				// writers
				if (state.compare_exchange_strong(
				      currentState, currentState | WriterWaitingFlag))
				{
					Debug::log("Trying to acquire writer lock {}, sleeping on "
					           "writer futex with value {}",
					           this,
					           writerEpoch);
					int result = writerNotify.wait(timeout, writerEpoch);

					// If we timed out, we may need to notify some other waiters
					if (result < 0)
					{
						// Get the new value of the word.  We're leaving the
						// waiting path so will need to clear the readers
						// waiting flag.
						currentState = state.load(std::memory_order_relaxed);
						do
						{
							// If someone else already cleared the
							// writer-waiting flag, they're responsible for
							// waking up everyone who might be blocked behind
							// us.  Leave them to it.
							if (!(currentState & WriterWaitingFlag))
							{
								return result;
							}
						} while (!state.compare_exchange_strong(
						  currentState, currentState & ~WriterWaitingFlag));

						// If the lock is still held by a writer, it will
						// handle waking readers, but we need to wake writers
						// to make sure that any pending writers re-set the
						// flag that we've just cleared.
						if (is_write_locked(currentState) ||
						    !are_readers_pending(currentState))
						{
							wake_writers();
							return result;
						}

						// If there are pending readers, wake some other threads
						if (are_readers_pending(currentState))
						{
							// There may be other writers blocked (the
							// writers-waiting flag is a flag, not a reference
							// count), so wake them.
							wake_writers();
							// If the last lock holder dropped the lock, future
							// readers may be blocked begin us.  The last
							// reader will have tried to wake us, but we missed
							// the notification, so wake readers.  If the lock
							// is still held, we can rely on whoever drops it
							// to wake readers.
							if (reader_lock_count(currentState) == 0)
							{
								wake_readers();
							}
						}
						return result;
					}
				}
			}
		}

		int unlock_reader()
		{
			auto     currentState = state.load(std::memory_order_relaxed);
			uint32_t desired;
			bool     wakeReaders;
			bool     wakeWriters;
			do
			{
				if (is_write_locked(currentState) ||
				    (reader_lock_count(currentState) == 0))
				{
					Debug::log("Invalid lock to unlock as reader: {}",
					           currentState);
					return -EINVAL;
				}
				desired     = currentState - 1;
				wakeReaders = false;
				wakeWriters = false;
				// If we are dropping the last read refcount, we may need to
				// wake readers.
				if (reader_lock_count(currentState) == 1)
				{
					// If there are writers pending, we need to wake them.
					if (are_writers_pending(currentState))
					{
						desired &= ~WriterWaitingFlag;
						wakeWriters = true;
					}
					// If there are readers blocked but not writers, we need to
					// wake them.
					else if (are_readers_pending(currentState))
					{
						desired &= ~ReaderWaitingFlag;
						wakeReaders = true;
					}
				}
			} while (!state.compare_exchange_strong(currentState, desired));
			if (wakeReaders)
			{
				wake_readers();
			}
			if (wakeWriters)
			{
				wake_writers();
			}
			return 0;
		}

		int unlock_writer()
		{
			auto currentState = state.load(std::memory_order_relaxed);
			if (!is_write_locked(currentState))
			{
				return -EINVAL;
			}
			uint32_t desired;
			bool     wakeReaders;
			bool     wakeWriters;
			do
			{
				desired     = currentState & ~ReadersMask;
				wakeReaders = false;
				wakeWriters = false;
				// If there are readers pending, we need to wake them.
				if (are_readers_pending(currentState))
				{
					desired &= ~ReaderWaitingFlag;
					wakeReaders = true;
				}
				// If there are writers blocked but not readers, we need to wake
				// them.
				else if (are_writers_pending(currentState))
				{
					desired &= ~WriterWaitingFlag;
					wakeWriters = true;
				}
			} while (!state.compare_exchange_strong(currentState, desired));

			if (wakeReaders)
			{
				wake_readers();
			}
			if (wakeWriters)
			{
				wake_writers();
			}
			return 0;
		}
	};
} // namespace

int read_write_lock_acquire_as_reader(Timeout                   *t,
                                      struct ReadWriteLockState *rwlockState)
{
	return static_cast<ReadWriteLock *>(rwlockState)->try_read_lock(t);
}

/**
 * Acquire a read-write lock in reader mode.  Returns 0 on success, or an error
 * result (negative `errno` number) from a futex wait on failure.
 */
int read_write_lock_acquire_as_writer(Timeout                   *t,
                                      struct ReadWriteLockState *rwlockState)
{
	return static_cast<ReadWriteLock *>(rwlockState)->try_write_lock(t);
}

/**
 * Release a read-write lock in writer mode.  Returns 0 on success.  Future
 * versions may return other negative values as errors.
 */
int read_write_lock_release_as_reader(struct ReadWriteLockState *rwlockState)
{
	return static_cast<ReadWriteLock *>(rwlockState)->unlock_reader();
}

/**
 * Release a read-write lock in reader mode.  Returns 0 on success.  Future
 * versions may return other negative values as errors.
 */
int read_write_lock_release_as_writer(struct ReadWriteLockState *rwlockState)
{
	return static_cast<ReadWriteLock *>(rwlockState)->unlock_writer();
}

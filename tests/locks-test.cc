// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Locks"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>
#include <locks.hh>
#include <thread.h>
#include <thread_pool.h>

using namespace CHERI;
using namespace thread_pool;

namespace
{

	FlagLock                  flagLock;
	FlagLockPriorityInherited flagLockPriorityInherited;
	TicketLock                ticketLock;

	cheriot::atomic<bool> modified;
	cheriot::atomic<int>  counter;

	/**
	 * Test that a lock meets the minimum requirements: it actually provides
	 * mutual exclusion.
	 */
	template<typename Lock>
	void test_lock(Lock &lock)
	{
		modified = false;
		debug_log("Acquiring lock in {}", __PRETTY_FUNCTION__);
		{
			LockGuard g{lock};
			async([&]() {
				LockGuard g{lock};
				modified = true;
			});
			sleep(2);
			TEST(modified == false,
			     "flag concurrently modified while lock is held!");
		}
		sleep(1);
		while (!modified)
		{
			debug_log("Other thread not finished, yielding");
			sleep(1);
		}
	}

	/// Test that try_lock fails after a timeout
	template<typename Lock>
	void test_trylock(Lock &lock)
	{
		LockGuard g{lock};
		debug_log("Trying to acquire already-held lock in {}",
		          __PRETTY_FUNCTION__);
		Timeout t{1};
		TEST(lock.try_lock(&t) == false,
		     "Trying to acquire lock spuriously succeeded");
		if constexpr (!std::is_same_v<Lock, FlagLockPriorityInherited>)
		{
			TEST(t.elapsed >= 1, "Sleep slept for {} ticks", t.elapsed);
		}
	}

	/**
	 * Test that destructing a lock automatically wakes up all waiters,
	 * failing them to acquire the lock.
	 */
	template<typename Lock>
	void test_destruct_lock_wake_up(Lock &lock)
	{
		modified = false;

		Timeout t{1};
		TEST(lock.try_lock(&t), "Failed to acquire uncontended lock");

		// Try to acquire the lock in a background thread
		async([&]() {
			// Make sure that we don't prevent the thread pool
			// making progress if this test fails.
			// The generous timer makes sure that we reach the
			// modified == true assert before the timeout.
			Timeout t2{20};
			TEST(lock.try_lock(&t2) == false,
			     "Lock acquisition should not succeed!");

			// When the lock is upgraded in destruction mode,
			// `lock` will return failure.
			modified = true;
		});

		// Give the thread a chance to run
		sleep(1);

		// Upgrade the lock to destruction mode
		lock.upgrade_for_destruction();

		// Give the waiter a chance to wake up
		sleep(1);

		// Check that the destruction mode woke up the waiters
		TEST(modified == true, "Destruction mode did not wake up waiters!");

		// Reset the lock in case other tests use it.
		// Note: in practice, lock.upgrade_for_destruction() would be
		// followed by a free() operation, not by unlock(). However
		// unlock() comes handy here to remove the destruction flag to
		// let other tests run properly with the same lock object
		lock.unlock();
	}

	/**
	 * Test that a lock with the destruction bit set cannot be acquired
	 * anymore.
	 *
	 * Note: here, plug at the C API to be able to check C error codes.
	 */
	void test_destruct_flag_lock_acquire()
	{
		static FlagLockState flagLockState;
		static FlagLockState priorityFlagLockState;

		Timeout t{5};
		int     ret = flaglock_trylock(&t, &flagLockState);
		TEST(ret == 0, "Flag lock trylock failed with error {}", ret);

		// Upgrade the lock to destruction mode
		flaglock_upgrade_for_destruction(&flagLockState);

		// Check that we now fail to grab the lock with the right error
		TEST(flaglock_trylock(&t, &flagLockState) == -ENOENT,
		     "Acquiring the lock did not fail with -ENOENT although it is in "
		     "destruction mode");

		// Now, do the same tests with the priority inheriting flag lock
		ret = flaglock_priority_inheriting_trylock(&t, &priorityFlagLockState);
		TEST(ret == 0,
		     "Priority inheriting flag lock trylock failed with error {}",
		     ret);

		flaglock_upgrade_for_destruction(&priorityFlagLockState);

		TEST(flaglock_priority_inheriting_trylock(&t, &priorityFlagLockState) ==
		       -ENOENT,
		     "Acquiring the lock did not fail with -ENOENT although it is in "
		     "destruction mode");
	}

	void test_recursive_mutex()
	{
		static RecursiveMutexState recursiveMutex;
		static std::atomic<bool>   done{false};
		debug_log("Testing recursive mutex");
		Timeout t{5};
		int     ret = recursivemutex_trylock(&t, &recursiveMutex);
		TEST(ret == 0, "Recursive mutex trylock failed with error {}", ret);
		debug_log("Aquiring recursive mutex again");
		ret = recursivemutex_trylock(&t, &recursiveMutex);
		TEST(ret == 0,
		     "Recursive mutex trylock failed on mutex owned by this thread "
		     "with error {}",
		     ret);
		// we don't expect above to block as there is no contention.
		TEST(t.elapsed == 0,
		     "Recursive mutex trylock slept for {} ticks",
		     t.elapsed);
		async([]() {
			Timeout t{0};
			debug_log(
			  "Trying to acquire recursive mutex in other thread (timeout 0)");
			int ret = recursivemutex_trylock(&t, &recursiveMutex);
			TEST(ret != 0,
			     "Recursive mutex trylock succeeded on mutex owned by another "
			     "thread");
			debug_log("Trying to acquire recursive mutex in other thread "
			          "(unlimited timeout)");
			t   = UnlimitedTimeout;
			ret = recursivemutex_trylock(&t, &recursiveMutex);
			TEST(ret == 0,
			     "Recursive mutex failed after mutex was unlocked by "
			     "another thread");
			debug_log("Other thread acquired recursive mutex");
			done = true;
		});
		// Give other thread a chance to run
		sleep(2);
		// Check that it hasn't acquired the lock yet as we still have it
		TEST(
		  done == false,
		  "Recursive mutex trylock succeeded on mutex owned by another thread");
		// Unlock once, should still not release the lock
		debug_log("Releasing recurisve mutex once");
		recursivemutex_unlock(&recursiveMutex);
		sleep(2);
		TEST(
		  done == false,
		  "Recursive mutex trylock succeeded on mutex owned by another thread");
		debug_log("Releasing recurisve mutex again");
		recursivemutex_unlock(&recursiveMutex);
		sleep(2);
		TEST(done == true,
		     "Recursive mutex acquire failed from other thread after mutex was "
		     "unlocked");
	}

	/**
	 * Test that the ticket lock gives the ordering guarantees that it should.
	 */
	void test_ticket_lock_ordering()
	{
		debug_log("Starting ticket-lock ordering tests");
		{
			LockGuard g{ticketLock};
			async([&]() {
				LockGuard g{ticketLock};
				TEST(counter == 0,
				     "Ticket lock acquired out of order, counter is {}, "
				     "expected 0",
				     counter.load());
				counter = 1;
			});
			async([&]() {
				sleep(5);
				LockGuard g{ticketLock};
				TEST(counter == 1,
				     "Ticket lock acquired out of order, counter is {}, "
				     "expected 1",
				     counter.load());
				counter = 2;
			});
			// Make sure both other threads are blocked on the ticket lock.
			sleep(10);
		}
		// We should not be allowed to run until both of the other threads have
		// run.
		LockGuard g{ticketLock};
		TEST(counter == 2,
		     "Ticket lock acquired out of order, counter is {}, expected 2",
		     counter.load());
	}

} // namespace

void test_locks()
{
	test_lock(flagLock);
	test_lock(flagLockPriorityInherited);
	test_lock(ticketLock);
	test_trylock(flagLock);
	test_trylock(flagLockPriorityInherited);
	test_destruct_lock_wake_up(flagLock);
	test_destruct_lock_wake_up(flagLockPriorityInherited);
	test_destruct_flag_lock_acquire();
	test_ticket_lock_ordering();
	test_recursive_mutex();
}

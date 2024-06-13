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
	 * Test that a lock actually provides mutual exclusion.
	 */
	template<typename Lock>
	void test_lock(Lock &lock)
	{
		modified = false;
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
#ifndef SIMULATION
			TEST(t.elapsed >= 1, "Sleep slept for {} ticks", t.elapsed);
#endif
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
			Timeout t2{25};
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
		sleep(5);

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
	 * Test that unlocking a flag lock works: once a lock is released, it
	 * can be re-acquired. This will fail if the unlock function does not
	 * properly clear bits.
	 */
	void test_flaglock_unlock()
	{
		// Check for the case where a lock was released with the
		// waiters bit set. Do not check the simple case without
		// waiters as this is covered by other tests (at least
		// `test_lock`).
		Timeout t{5};
		TEST(flagLock.try_lock(&t), "Failed to acquire uncontended lock");
		counter = 0;
		async([&]() {
			Timeout t2{1};
			TEST(flagLock.try_lock(&t2) == false,
			     "Lock acquisition should not succeed!");
			counter++;
		});
		do
		{
			debug_log("Other thread not finished, yielding");
			sleep(1);
		} while (counter.load() == 0);
		flagLock.unlock();

		TEST(flagLock.try_lock(&t),
		     "Failed to acquire uncontended lock after unlock when the waiters "
		     "bit was previously set");
		flagLock.unlock();
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

		// Upgrade the lock to destruction mode. No need to acquire the
		// lock for that.
		flaglock_upgrade_for_destruction(&flagLockState);

		// Check that we now fail to grab the lock with the right error
		Timeout t{5};
		TEST(flaglock_trylock(&t, &flagLockState) == -ENOENT,
		     "Acquiring the lock did not fail with -ENOENT although it is in "
		     "destruction mode");

		// Now, do the same tests with the priority inheriting flag lock
		flaglock_upgrade_for_destruction(&priorityFlagLockState);

		TEST(flaglock_priority_inheriting_trylock(&t, &priorityFlagLockState) ==
		       -ENOENT,
		     "Acquiring the lock did not fail with -ENOENT although it is in "
		     "destruction mode");
	}

	/**
	 * Test that the destruction bit is preserved when unlocking.
	 *
	 * Note: here, plug at the C API to be able to check C error codes.
	 */
	void test_destruct_flag_lock_unlock()
	{
		// Only test without priority inheriting for code size reasons,
		// but both should behave identically
		static FlagLockState flagLockState;

		Timeout t{5};
		int     ret = flaglock_trylock(&t, &flagLockState);
		TEST(ret == 0, "Flag lock trylock failed with error {}", ret);

		// Upgrade the lock to destruction mode
		flaglock_upgrade_for_destruction(&flagLockState);

		// Now unlock the lock
		flaglock_unlock(&flagLockState);

		// Check that the destruction bit is still set (by trying to
		// grab the lock again - it should fail with -ENOENT)
		TEST(flaglock_trylock(&t, &flagLockState) == -ENOENT,
		     "Unlocking unsets the destruction bit of flag lock");
	}

	/**
	 * Test that `get_owner_thread_id` returns the thread ID of the owner
	 * of the lock.
	 */
	void test_get_owner_thread_id(FlagLockPriorityInherited &lock)
	{
		debug_log("Testing that `get_owner_thread_id` works.");

		TEST(lock.get_owner_thread_id() == 0,
		     "`get_owner_thread_id` does not return 0 when called on an unheld "
		     "lock");

		modified = false;
		LockGuard g{lock};
		uint16_t  ownerThreadId = thread_id_get();
		TEST(lock.get_owner_thread_id() == ownerThreadId,
		     "`get_owner_thread_id` does not return the thread ID of the lock "
		     "owner");

		async([ownerThreadId, &lock]() {
			TEST(thread_id_get() != ownerThreadId,
			     "Async has the same thread ID as the main thread");
			TEST(lock.get_owner_thread_id() == ownerThreadId,
			     "`get_owner_thread_id` does not return the thread ID of the "
			     "lock owner when called from a non-owning thread");
			modified = true;
		});

		sleep(1);
		while (!modified)
		{
			debug_log("Other thread not finished, yielding");
			sleep(1);
		}
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
		counter = 0;
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

	/**
	 * Test ticket lock behavior on overflow.
	 *
	 * Note: here, plug at the C API to be able to set the lock's starting
	 * state.  Ideally we would be able to do that externally from the C++
	 * API without setting a specific internal state, but this requires us
	 * to loop `lock` and `unlock` for about 2^32 iterations, which takes
	 * too long on both the simulator and the FPGA.
	 */
	void test_ticket_lock_overflow()
	{
		static TicketLockState ticketLockState;
		counter = 0;

		// Put the ticket lock in an unlocked state where it is about
		// to overflow.
		ticketLockState.current = std::numeric_limits<uint32_t>::max() - 2;
		ticketLockState.next    = std::numeric_limits<uint32_t>::max() - 2;

		// Take the lock on the main thread.
		ticketlock_lock(&ticketLockState);
		// Now we should have (current: max-2, next: max-1).

		// Now create two more threads which will try to get hold of
		// the lock, thereby overflowing the `next` counter.
		async([&]() {
			ticketlock_lock(&ticketLockState);
			// Now we should have (current: max-2, next: max).
			counter++;
			ticketlock_unlock(&ticketLockState);
			// Now we should have (current: max, next: 0).
		});
		async([&]() {
			ticketlock_lock(&ticketLockState);
			// We just overflowed.
			// Now we should have (current: max-2, next: 0).
			counter++;
			ticketlock_unlock(&ticketLockState);
			// Now we should have (current: 0, next: 0).
		});

		// Give the threads a chance to run and increment `next`.
		sleep(20);

		// Release the lock on the main thread.
		ticketlock_unlock(&ticketLockState);
		// Now we should have (current: max-1, next: 0).

		// Give the threads a chance to wake up.
		sleep(20);

		// The final state should be (current: 0, next: 0).

		// Check that unlocking woke up the waiters.  Subsequent tests
		// will generally fail if this fails, because one or all of the
		// two threads may still be live and deadlocked.
		TEST(counter.load() == 2,
		     "Ticket lock deadlocked because of overflow, expected 2 threads "
		     "to wake up, got {}",
		     counter.load());
	}

} // namespace

void test_locks()
{
	test_lock(flagLock);
	test_lock(flagLockPriorityInherited);
	test_lock(ticketLock);
	test_get_owner_thread_id(flagLockPriorityInherited);
	test_flaglock_unlock();
	test_trylock(flagLock);
	test_trylock(flagLockPriorityInherited);
	test_destruct_lock_wake_up(flagLock);
	test_destruct_lock_wake_up(flagLockPriorityInherited);
	test_destruct_flag_lock_acquire();
	test_destruct_flag_lock_unlock();
	test_ticket_lock_ordering();
	test_ticket_lock_overflow();
	test_recursive_mutex();
}

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

	FlagLock   flagLock;
	TicketLock ticketLock;

	_Atomic(bool) modified;
	_Atomic(int)  counter;

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
		TEST(t.elapsed >= 1, "Sleep slept for {} ticks", t.elapsed);
	}

} // namespace

void test_locks()
{
	test_lock(flagLock);
	test_lock(ticketLock);
	test_trylock(flagLock);

	// Test that the ticket lock gives the ordering guarantees that it should.
	{
		LockGuard g{ticketLock};
		async([&]() {
			LockGuard g{ticketLock};
			TEST(counter == 0,
			     "Ticket lock acquired out of order, counter is {}, expected 0",
			     counter);
			counter = 1;
		});
		async([&]() {
			sleep(5);
			LockGuard g{ticketLock};
			TEST(counter == 1,
			     "Ticket lock acquired out of order, counter is {}, expected 1",
			     counter);
			counter = 2;
		});
		// Make sure both other threads are blocked on the ticket lock.
		sleep(10);
	}
	// We should not be allowed to run until both of the other threads have run.
	LockGuard g{ticketLock};
	TEST(counter == 2,
	     "Ticket lock acquired out of order, counter is {}, expected 2",
	     counter);
}

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define CHERIOT_NO_AMBIENT_MALLOC
#define CHERIOT_NO_NEW_DELETE
#include "../switcher/tstack.h"
#include "multiwait.h"
#include "plic.h"
#include "thread.h"
#include "timer.h"
#include <cdefs.h>
#include <cheri.hh>
#include <compartment.h>
#include <futex.h>
#include <interrupt.h>
#include <locks.hh>
#include <new>
#include <priv/riscv.h>
#include <riscvreg.h>
#include <simulator.h>
#include <stdint.h>
#include <stdlib.h>
#include <thread.h>
#include <token.h>

using namespace CHERI;

#ifdef SIMULATION
#	include <platform-simulation_exit.hh>

/**
 * Exit simulation, reporting the error code given as the argument.
 */
int scheduler_simulation_exit(uint32_t code)
{
	platform_simulation_exit(code);
	return -EPROTO;
}
#endif

/**
 * The value of the cycle counter at the last scheduling event.
 */
static uint64_t cyclesAtLastSchedulingEvent;

namespace
{
	/**
	 * Priority-sorted list of threads waiting for a futex.
	 */
	Thread *futexWaitingList;

	/**
	 * The value used for priority-boosting futexes that are not actually
	 * boosting a thread currently.
	 */
	constexpr uint16_t FutexBoostNotThread =
	  std::numeric_limits<uint16_t>::max();

	/**
	 * Returns the boosted priority provided by waiters on a futex.
	 *
	 * This finds the maximum priority of all threads that are priority
	 * boosting the thread identified by `threadID`.  Callers may be about to
	 * add a new thread to that list and so another priority can be provided,
	 * which will be used if it is larger than any of the priorities of the
	 * other waiters.
	 */
	uint8_t priority_boost_for_thread(uint16_t threadID, uint8_t priority = 0)
	{
		Thread::walk_thread_list(futexWaitingList, [&](Thread *thread) {
			if ((thread->futexPriorityInheriting) &&
			    (thread->futexPriorityBoostedThread == threadID))
			{
				priority = std::max(priority, thread->priority_get());
			}
		});
		return priority;
	}

	/**
	 * If a new futex_wait has come in with an updated owner for a lock, update
	 * all of the blocking threads to boost the new owner.
	 */
	void priority_boost_update(ptraddr_t key, uint16_t threadID)
	{
		Thread::walk_thread_list(futexWaitingList, [&](Thread *thread) {
			if ((thread->futexPriorityInheriting) &&
			    (thread->futexWaitAddress = key))
			{
				thread->futexPriorityBoostedThread = threadID;
			}
		});
	}

	/**
	 * Reset the boosting thread for all threads waiting on the current futex
	 * to not boosting anything when the owning thread wakes.
	 *
	 * There is a potential race here because the `futex_wait` call happens
	 * after unlocking the futex.  This means that another thread may come in
	 * and acquire a lock and set itself as the owner before the update.  We
	 * therefore need to update waiting threads only if they are boosting the
	 * thread that called wake, not any other thread.
	 */
	void priority_boost_reset(ptraddr_t key, uint16_t threadID)
	{
		Thread::walk_thread_list(futexWaitingList, [&](Thread *thread) {
			if ((thread->futexPriorityInheriting) &&
			    (thread->futexWaitAddress = key))
			{
				if (thread->futexPriorityBoostedThread == threadID)
				{
					thread->futexPriorityBoostedThread = FutexBoostNotThread;
				}
			}
		});
	}

	/**
	 * Constant value used to represent an unbounded sleep.
	 */
	static constexpr auto UnboundedSleep = std::numeric_limits<uint32_t>::max();

	/**
	 * Helper that wakes a set of up to `count` threads waiting on the futex
	 * whose address is given by the `key` parameter.
	 *
	 * The return values are:
	 *
	 *  - Whether a higher-priority thread has been woken, which would trigger
	 *    an immediate yield.
	 *  - Whether this futex was using priority inheritance and so should be
	 *    dropped back to the previous priority.
	 *  - The number of sleeper that were awoken.
	 */
	std::tuple<bool, bool, int>
	futex_wake(ptraddr_t key,
	           uint32_t  count = std::numeric_limits<uint32_t>::max())
	{
		bool shouldYield                    = false;
		bool shouldRecalculatePriorityBoost = false;
		// The number of threads that we've woken, this is the return value on
		// success.
		int woke = 0;
		Thread::walk_thread_list(
		  futexWaitingList,
		  [&](Thread *thread) {
			  if (thread->futexWaitAddress == key)
			  {
				  shouldRecalculatePriorityBoost |=
				    thread->futexPriorityInheriting;
				  shouldYield = thread->ready(Thread::WakeReason::Futex);
				  count--;
				  woke++;
			  }
		  },
		  [&]() { return count == 0; });

		if (count > 0)
		{
			auto multiwaitersWoken =
			  MultiWaiterInternal::wake_waiters(key, count);
			count -= multiwaitersWoken;
			woke += multiwaitersWoken;
			shouldYield |= (multiwaitersWoken > 0);
		}
		return {shouldYield, shouldRecalculatePriorityBoost, woke};
	}

} // namespace

namespace sched
{
	using namespace priv;

	/**
	 * Reserved spaces for thread blocks and the event signaling for external
	 * interrupts. These will be used for in-place new on the first sched entry.
	 */
	using ThreadSpace = char[sizeof(Thread)];
	alignas(Thread) ThreadSpace threadSpaces[CONFIG_THREADS_NUM];

	/**
	 * Return the thread pointer for the specified thread ID.
	 */
	Thread *get_thread(uint16_t threadId)
	{
		if (threadId > CONFIG_THREADS_NUM || threadId == 0)
		{
			return nullptr;
		}
		return &(reinterpret_cast<Thread *>(threadSpaces))[threadId - 1];
	}

	[[cheri::interrupt_state(disabled)]] int __cheri_compartment("scheduler")
	  scheduler_entry(const ThreadLoaderInfo *info)
	{
		Debug::Invariant(Capability{info}.length() ==
		                   sizeof(*info) * CONFIG_THREADS_NUM,
		                 "Thread info is {} bytes, expected {} for {} threads",
		                 Capability{info}.length(),
		                 sizeof(*info) * CONFIG_THREADS_NUM,
		                 CONFIG_THREADS_NUM);

		for (size_t i = 0; auto *threadSpace : threadSpaces)
		{
			Debug::log("Created thread for trusted stack {}",
			           info[i].trustedStack);
			Thread *th = new (threadSpace)
			  Thread(info[i].trustedStack, i + 1, info[i].priority);
			th->ready(Thread::WakeReason::Timer);
			i++;
		}

		InterruptController::master_init();
		Timer::interrupt_setup();

		return 0;
	}

	static void __dead2 sched_panic(size_t mcause, size_t mepc, size_t mtval)
	{
		size_t capcause = mtval & 0x1f;
		size_t badcap   = (mtval >> 5) & 0x3f;
		Debug::log("CRASH! exception level {}, mcause {}, mepc {}, "
		           "capcause {}, badcap {}\n",
		           static_cast<uint32_t>(ExceptionGuard::exceptionLevel),
		           static_cast<uint32_t>(mcause),
		           static_cast<uint32_t>(mepc),
		           static_cast<uint32_t>(capcause),
		           badcap);

#ifdef SIMULATION
		// If we're in simulation, exit here
		platform_simulation_exit(1);
#endif

		for (;;)
		{
			wfi();
		}
	}

	[[cheri::interrupt_state(disabled)]] TrustedStack *
	  __cheri_compartment("scheduler") exception_entry(TrustedStack *sealedTStack,
	                                               size_t        mcause,
	                                               size_t        mepc,
	                                               size_t        mtval)
	{
		if constexpr (DebugScheduler)
		{
			/* Ensure that we got here from an IRQ-s deferred context */
			Capability returnAddress{__builtin_return_address(0)};
			Debug::Assert(
			  returnAddress.type() == CheriSealTypeReturnSentryDisabling,
			  "Scheduler exception_entry called from IRQ-enabled context");
		}

		// The cycle count value the last time the scheduler returned.
		bool schedNeeded;
		if constexpr (Accounting)
		{
			uint64_t  currentCycles = rdcycle64();
			auto     *thread        = Thread::current_get();
			uint64_t &threadCycleCounter =
			  thread ? thread->cycles : Thread::idleThreadCycles;
			auto elapsedCycles = currentCycles - cyclesAtLastSchedulingEvent;
			threadCycleCounter += elapsedCycles;
		}

		ExceptionGuard g{[=]() { sched_panic(mcause, mepc, mtval); }};

		bool tick = false;
		switch (mcause)
		{
			// Explicit yield call
			case MCAUSE_ECALL_MACHINE:
			{
				schedNeeded           = true;
				Thread *currentThread = Thread::current_get();
				tick = currentThread && currentThread->is_ready();
				break;
			}
			case MCAUSE_INTR | MCAUSE_MTIME:
				schedNeeded = true;
				tick        = true;
				break;
			case MCAUSE_INTR | MCAUSE_MEXTERN:
				schedNeeded = false;
				InterruptController::master().do_external_interrupt().and_then(
				  [&](uint32_t &word) {
					  // Increment the futex word so that anyone preempted on
					  // the way into the scheduler sleeping on its old value
					  // will still see this update.
					  word++;
					  // Wake anyone sleeping on this futex.  Interrupt futexes
					  // are not priority inheriting.
					  std::tie(schedNeeded, std::ignore, std::ignore) =
					    futex_wake(Capability{&word}.address());
				  });
				tick = schedNeeded;
				break;
			case MCAUSE_THREAD_EXIT:
				// Make the current thread non-runnable.
				if (Thread::exit())
				{
#ifdef SIMULATION
					// If we have no threads left (not counting the idle
					// thread), exit.
					platform_simulation_exit(0);
#endif
				}
				// We cannot continue exiting this thread, make sure we will
				// pick a new one.
				schedNeeded  = true;
				tick         = true;
				sealedTStack = nullptr;
				break;
			default:
				sched_panic(mcause, mepc, mtval);
		}
		if (tick || !Thread::any_ready())
		{
			Timer::expiretimers();
		}
		auto newContext =
		  schedNeeded ? Thread::schedule(sealedTStack) : sealedTStack;
#if 0
		Debug::log("Thread: {}",
		           Thread::current_get() ? Thread::current_get()->id_get() : 0);
#endif
		Timer::update();

		if constexpr (Accounting)
		{
			cyclesAtLastSchedulingEvent = rdcycle64();
		}
		return newContext;
	}

	/**
	 * Helper template to dispatch an operation to a typed value.  The first
	 * argument is a sealed capability provided by the caller.  The second is a
	 * callable object that takes a reference to the unsealed object of the
	 * correct type.  The return type of the lambda is either a single integer
	 * or a pair of an integer and a boolean.  The integer value is simply
	 * returned.  If the boolean is present then it is used to determine
	 * whether to yield at the end.
	 */
	template<typename T>
	int typed_op(void *sealed, auto &&fn)
	{
		auto *unsealed = T::template unseal<T>(sealed);
		// If we can't unseal the sealed capability and have it be of the
		// correct type then return an error.
		if (!unsealed)
		{
			return -EINVAL;
		}
		// Does the implementation return a simple `int`?  If so, just tail call
		// it.
		if constexpr (std::is_same_v<decltype(fn(std::declval<T &>())), int>)
		{
			return fn(*unsealed);
		}
		else
		{
			auto [ret, shouldYield] = fn(*unsealed);

			if (shouldYield)
			{
				yield();
			}

			return ret;
		}
	}

	/// Lock used to serialise deallocations.
	FlagLock deallocLock;

	/// Helper to safely deallocate an instance of `T`.
	template<typename T>
	int deallocate(SObjStruct *heapCapability, void *objectPtr)
	{
		static_assert(T::IsDynamic);

		// Acquire the lock and hold it. We need to be careful of two attempts
		// to free the same object racing, so we cause others to back up behind
		// this one.  They will then fail in the unseal operation.
		LockGuard g{deallocLock};
		return typed_op<T>(objectPtr, [&](T &unsealed) {
			SObj object = static_cast<SObj>(objectPtr);
			if (int ret = token_obj_can_destroy(
			      heapCapability, T::sealing_type(), object);
			    ret != 0)
			{
				return ret;
			}
			unsealed.~T();
			return token_obj_destroy(heapCapability, T::sealing_type(), object);
		});
	}

} // namespace sched

using namespace sched;

// thread APIs
SystickReturn __cheri_compartment("scheduler") thread_systemtick_get()
{
	uint64_t      ticks = Thread::ticksSinceBoot;
	uint32_t      hi    = ticks >> 32;
	uint32_t      lo    = ticks;
	SystickReturn ret   = {.lo = lo, .hi = hi};

	return ret;
}

__cheriot_minimum_stack(0x90) int __cheri_compartment("scheduler")
  thread_sleep(Timeout *timeout, uint32_t flags)
{
	STACK_CHECK(0x90);
	if (!check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	// Debug::log("Thread {} sleeping for {} ticks",
	//  Thread::current_get()->id_get(), timeout->remaining);
	Thread *current = Thread::current_get();
	current->suspend(timeout, nullptr, true, !(flags & ThreadSleepNoEarlyWake));
	return 0;
}

__cheriot_minimum_stack(0xb0) int futex_timed_wait(Timeout        *timeout,
                                                   const uint32_t *address,
                                                   uint32_t        expected,
                                                   uint32_t        flags)
{
	STACK_CHECK(0xb0);
	if (!check_timeout_pointer(timeout) ||
	    !check_pointer<PermissionSet{Permission::Load}>(address))
	{
		Debug::log("futex_timed_wait: invalid timeout or address");
		return -EINVAL;
	}
	// If the address does not contain the expected value then this call
	// raced with an update in another thread, return success immediately.
	if (*address != expected)
	{
		Debug::log("futex_timed_wait: skip wait {} != {}", *address, expected);
		return 0;
	}
	Thread *currentThread = Thread::current_get();
	Debug::log("Thread {} waiting on futex {} for {} ticks",
	           currentThread->id_get(),
	           address,
	           timeout->remaining);
	bool      isPriorityInheriting         = flags & FutexPriorityInheritance;
	ptraddr_t key                          = Capability{address}.address();
	currentThread->futexWaitAddress        = key;
	currentThread->futexPriorityInheriting = isPriorityInheriting;
	Thread  *owningThread                  = nullptr;
	uint16_t owningThreadID;
	if (isPriorityInheriting)
	{
		// For PI futexes, the low 16 bits store the thread ID.
		owningThreadID = *address;
		owningThread   = get_thread(owningThreadID);
		// If we try to block ourself, that's a mistake.
		if ((owningThread == currentThread) || (owningThread == nullptr))
		{
			Debug::log("futex_timed_wait: thread {} acquiring PI futex with "
			           "invalid owning thread {}",
			           currentThread->id_get(),
			           owningThreadID);
			return -EINVAL;
		}
		Debug::log("Thread {} boosting priority of {} for futex {}",
		           currentThread->id_get(),
		           owningThread->id_get(),
		           key);
		// If other threads are boosting either the wrong thread or are
		// priority boosting but haven't managed to acquire the lock, update
		// their target.
		priority_boost_update(key, owningThreadID);
		owningThread->priority_boost(priority_boost_for_thread(
		  owningThreadID, currentThread->priority_get()));
	}
	currentThread->suspend(timeout, &futexWaitingList);
	bool timedout                   = currentThread->futexWaitAddress == 0;
	currentThread->futexWaitAddress = 0;
	if (isPriorityInheriting)
	{
		Debug::log("Undoing priority boost of {} by {}",
		           owningThread->id_get(),
		           currentThread->id_get());
		// Recalculate the priority boost from the remaining waiters, if any.
		owningThread->priority_boost(priority_boost_for_thread(owningThreadID));
	}
	// If we woke up from a timer, report timeout.
	if (timedout)
	{
		return -ETIMEDOUT;
	}
	// If the memory for the futex was deallocated out from under us,
	// return an error.
	if (!Capability{address}.is_valid())
	{
		Debug::log(
		  "futex_timed_wait: futex address {} is invalid (deallocated?)",
		  address);
		return -EINVAL;
	}
	Debug::log("Thread {} ({}) woke after waiting on futex {}",
	           currentThread->id_get(),
	           currentThread,
	           address);
	return 0;
}

__cheriot_minimum_stack(0xa0) int futex_wake(uint32_t *address, uint32_t count)
{
	STACK_CHECK(0xa0);
	if (!check_pointer<PermissionSet{Permission::Store}>(address))
	{
		return -EINVAL;
	}
	ptraddr_t key = Capability{address}.address();

	auto [shouldYield, shouldResetPrioirity, woke] = futex_wake(key, count);

	// If this futex wake is dropping a priority boost, reset the boost.
	if (shouldResetPrioirity)
	{
		Thread *currentThread = Thread::current_get();
		// We are removing ourself from the priority boost from *this* futex,
		// we may still be boosted by another futex, but we have just dropped
		// the lock and so we should not be boosted so clear this thread as the
		// target for other priority boosts.
		priority_boost_reset(key, currentThread->id_get());
		// If we have nested priority-inheriting locks, we may have dropped the
		// inner one but still hold the outer one.  In this case, we need to
		// keep the priority boost.  Similarly, if we've done a notify-one
		// operation but two threads were blocked on a priority-inheriting
		// futex, then we need to keep the priority boost from the other
		// threads.
		currentThread->priority_boost(
		  priority_boost_for_thread(currentThread->id_get()));
		// If we have dropped priority below that of another runnable thread, we
		// should yield now.
		shouldYield |= !currentThread->is_highest_priority();
	}

	if (shouldYield)
	{
		yield();
	}

	return woke;
}

__cheriot_minimum_stack(0x60) int multiwaiter_create(
  Timeout           *timeout,
  struct SObjStruct *heapCapability,
  MultiWaiter      **ret,
  size_t             maxItems)
{
	STACK_CHECK(0x60);
	int error;
	// Don't bother checking if timeout is valid, the allocator will check for
	// us.
	auto mw =
	  MultiWaiterInternal::create(timeout, heapCapability, maxItems, error);
	if (!mw)
	{
		return error;
	}

	// This can trap, but only if the caller has provided a bad pointer.
	// In this case, the caller can leak memory, but only memory allocated
	// against its own quota.
	*reinterpret_cast<void **>(ret) = mw;

	return 0;
}

__cheriot_minimum_stack(0x70) int multiwaiter_delete(
  struct SObjStruct *heapCapability,
  MultiWaiter       *mw)
{
	STACK_CHECK(0x70);
	return deallocate<MultiWaiterInternal>(heapCapability, mw);
}

__cheriot_minimum_stack(0xc0) int multiwaiter_wait(Timeout           *timeout,
                                                   MultiWaiter       *waiter,
                                                   EventWaiterSource *events,
                                                   size_t newEventsCount)
{
	STACK_CHECK(0xc0);
	return typed_op<MultiWaiterInternal>(waiter, [&](MultiWaiterInternal &mw) {
		if (newEventsCount > mw.capacity())
		{
			Debug::log("Too many events");
			return -EINVAL;
		}
		// We don't need to worry about overflow here because we have ensured
		// newEventsCount is very small.
		if (!check_pointer<PermissionSet{Permission::Load,
		                                 Permission::Store,
		                                 Permission::LoadStoreCapability}>(
		      events, newEventsCount * sizeof(newEventsCount)))
		{
			Debug::log("Invalid new events pointer: {}", events);
			return -EINVAL;
		}
		if (!check_timeout_pointer(timeout))
		{
			return -EINVAL;
		}
		switch (mw.set_events(events, newEventsCount))
		{
			case MultiWaiterInternal::EventOperationResult::Error:
				Debug::log("Adding events returned error");
				return -EINVAL;
			case MultiWaiterInternal::EventOperationResult::Sleep:
				Debug::log("Sleeping for {} ticks", timeout->remaining);
				if (timeout->may_block())
				{
					mw.wait(timeout);
					// If we yielded then it's possible for either of the
					// pointers that we were passed to have been freed out from
					// under us.
					if (!Capability{&mw}.is_valid() ||
					    !Capability{events}.is_valid())
					{
						return -EINVAL;
					}
				}
				[[fallthrough]];
			case MultiWaiterInternal::EventOperationResult::Wake:
				// If we didn't find any events, then we timed out.  We may
				// still have timed out but received some events in between
				// being rescheduled and being run, but don't count that as a
				// timeout because it's not helpful to the user.
				if (!mw.get_results(events, newEventsCount))
				{
					return -ETIMEDOUT;
				}
		}
		return 0;
	});
}

namespace
{
	/**
	 * An interrupt capability.
	 */
	struct InterruptCapability : Handle</*IsDynamic=*/false>
	{
		/**
		 * Sealing type used by `Handle`.
		 */
		static SKey sealing_type()
		{
			return STATIC_SEALING_TYPE(InterruptKey);
		}

		/**
		 * The public structure state.
		 */
		InterruptCapabilityState state;
	};
} // namespace

[[cheri::interrupt_state(disabled)]] __cheriot_minimum_stack(
  0x30) const uint32_t *interrupt_futex_get(struct SObjStruct *sealed)
{
	STACK_CHECK(0x30);
	auto *interruptCapability =
	  InterruptCapability::unseal<InterruptCapability>(sealed);
	uint32_t *result = nullptr;
	if (interruptCapability && interruptCapability->state.mayWait)
	{
		InterruptController::master()
		  .futex_word_for_source(interruptCapability->state.interruptNumber)
		  .and_then([&](uint32_t &word) {
			  Capability capability{&word};
			  capability.permissions() &=
			    {Permission::Load, Permission::Global};
			  result = capability.get();
		  });
	}
	return result;
}

[[cheri::interrupt_state(disabled)]] __cheriot_minimum_stack(
  0x20) int interrupt_complete(struct SObjStruct *sealed)
{
	STACK_CHECK(0x20);
	auto *interruptCapability =
	  InterruptCapability::unseal<InterruptCapability>(sealed);
	if (interruptCapability && interruptCapability->state.mayComplete)
	{
		InterruptController::master().interrupt_complete(
		  interruptCapability->state.interruptNumber);
		return 0;
	}
	return -EPERM;
}

uint16_t thread_count()
{
	return CONFIG_THREADS_NUM;
}

#ifdef SCHEDULER_ACCOUNTING
[[cheri::interrupt_state(disabled)]] uint64_t thread_elapsed_cycles_idle()
{
	return Thread::idleThreadCycles;
}

[[cheri::interrupt_state(disabled)]] uint64_t thread_elapsed_cycles_current()
{
	// Calculate the number of cycles not yet reported to the current thread.
	uint64_t currentCycles = rdcycle64();
	currentCycles -= cyclesAtLastSchedulingEvent;
	// Report the number of cycles accounted to this thread, plus the number
	// that have occurred in the current quantum.
	return Thread::current_get()->cycles + currentCycles;
}
#endif

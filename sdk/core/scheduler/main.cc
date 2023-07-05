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
/**
 * This is a special MMIO register. Writing an LSB of 1 terminates
 * simulation. The upper 31 bits can pass extra metadata. We use all 0s
 * to indicates success.
 *
 * This symbol doesn't need to be exported. The simulator is clever
 * enough to find it even if it's local.
 */
volatile uint32_t tohost[2];

/**
 * Exit simulation, reporting the error code given as the argument.
 */
void simulation_exit(uint32_t code)
{
	tohost[0] = 0x1 | (code << 1);
	tohost[1] = 0;
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
	sched::Thread *futexWaitingList;

	/**
	 * Constant value used to represent an unbounded sleep.
	 */
	static constexpr auto UnboundedSleep = std::numeric_limits<uint32_t>::max();

	/**
	 * Helper that wakes a set of up to `count` threads waiting on the futex
	 * whose address is given by the `key` parameter.
	 */
	std::pair<bool, int>
	futex_wake(ptraddr_t key,
	           uint32_t  count = std::numeric_limits<uint32_t>::max())
	{
		bool shouldYield = false;
		// The number of threads that we've woken, this is the return value on
		// success.
		int woke = 0;
		sched::Thread::walk_thread_list(
		  futexWaitingList,
		  [&](sched::Thread *thread) {
			  if (thread->futexWaitAddress == key)
			  {
				  shouldYield = thread->ready(sched::Thread::WakeReason::Futex);
				  count--;
				  woke++;
			  }
		  },
		  [&]() { return count == 0; });

		if (count > 0)
		{
			auto multiwaitersWoken =
			  sched::MultiWaiter::wake_waiters(key, count);
			count -= multiwaitersWoken;
			woke += multiwaitersWoken;
			shouldYield |= (multiwaitersWoken > 0);
		}
		return {shouldYield, woke};
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

	[[cheri::interrupt_state(disabled)]] void __cheri_compartment("sched")
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

		// If we're in simulation, exit here
		simulation_exit(1);

		for (;;)
			wfi();
	}

	[[cheri::interrupt_state(disabled)]] TrustedStack *
	  __cheri_compartment("sched") exception_entry(TrustedStack *sealedTStack,
	                                               size_t        mcause,
	                                               size_t        mepc,
	                                               size_t        mtval)
	{
		// The cycle count value the last time the scheduler returned.
		bool schedNeeded;
		if constexpr (sched::Accounting)
		{
			uint64_t  currentCycles = rdcycle64();
			auto     *thread        = Thread::current_get();
			uint64_t &threadCycleCounter =
			  thread ? thread->cycles : Thread::idleThreadCycles;
			auto elapsedCycles = currentCycles - cyclesAtLastSchedulingEvent;
			threadCycleCounter += elapsedCycles;
		}

		ExceptionGuard g{[=]() { sched_panic(mcause, mepc, mtval); }};

		switch (mcause)
		{
			// Explicit yield call
			case MCAUSE_ECALL_MACHINE:
				schedNeeded = true;
				break;
			case MCAUSE_INTR | MCAUSE_MTIME:
				Timer::do_interrupt();
				schedNeeded = true;
				break;
			case MCAUSE_INTR | MCAUSE_MEXTERN:
				schedNeeded = false;
				InterruptController::master().do_external_interrupt().and_then(
				  [&](uint32_t &word) {
					  // Increment the futex word so that anyone preempted on
					  // the way into the scheduler sleeping on its old value
					  // will still see this update.
					  word++;
					  // Wake anyone sleeping on this futex.
					  std::tie(schedNeeded, std::ignore) =
					    futex_wake(Capability{&word}.address());
				  });
				break;
			case MCAUSE_THREAD_EXIT:
				// Make the current thread non-runnable.
				if (Thread::exit())
				{
					// If we have no threads left (not counting the idle
					// thread), exit.
					simulation_exit(0);
				}
				// We cannot continue exiting this thread, make sure we will
				// pick a new one.
				schedNeeded  = true;
				sealedTStack = nullptr;
				break;
			default:
				sched_panic(mcause, mepc, mtval);
		}
		auto newContext =
		  schedNeeded ? Thread::schedule(sealedTStack) : sealedTStack;

		if constexpr (sched::Accounting)
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
		auto *unsealed = Handle::unseal<T>(sealed);
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
				Thread::yield_interrupt_enabled();
			}

			return ret;
		}
	}

	/**
	 * Check that a timeout pointer is usable.
	 */
	bool check_timeout_pointer(Timeout *timeout)
	{
		return check_pointer<PermissionSet{Permission::Load,
		                                   Permission::Store}>(timeout);
	}

	/// Lock used to serialise deallocations.
	FlagLock deallocLock;

	/// Helper to safely deallocate an instance of `T`.
	template<typename T>
	int deallocate(SObjStruct *heapCapability, void *object)
	{
		// Acquire the lock and hold it. We need to be careful of two attempts
		// to free the same object racing, so we cause others to back up behind
		// this one.  They will then fail in the unseal operation.
		LockGuard g{deallocLock};
		return typed_op<T>(object, [&](T &unsealed) {
			if (int ret = heap_can_free(heapCapability, &unsealed); ret != 0)
			{
				return ret;
			}
			unsealed.~T();
			heap_free(heapCapability, &unsealed);
			return 0;
		});
	}

	/**
	 * Return path from `*_create` functions.  Performs a return check with
	 * interrupts disabled and stores the object managed by `object` via `ret`
	 * (consuming ownership) if `ret` has valid permissions.  Returns the value
	 * that the caller should return to the public API.
	 *
	 * This function runs with interrupts disabled so that the majority of
	 * `*_create` can have them enabled.
	 */
	template<typename T>
	[[cheri::interrupt_state(disabled)]] int write_result(void         **ret,
	                                                      HeapObject<T> &object)
	{
		if (!check_pointer<PermissionSet{Permission::Store,
		                                 Permission::LoadStoreCapability}>(ret))
		{
			return -EINVAL;
		}

		*ret = compart_seal(object.release());
		return 0;
	}

} // namespace sched

using namespace sched;

// queue APIs
int __cheri_compartment("sched") queue_create(Timeout           *timeout,
                                              struct SObjStruct *heapCapability,
                                              void             **ret,
                                              size_t             itemSize,
                                              size_t             maxNItems)
{
	HeapBuffer storage{timeout, heapCapability, itemSize, maxNItems};

	if (!storage)
	{
		return -ENOMEM;
	}

	HeapObject<Queue> queue{
	  timeout, heapCapability, std::move(storage), itemSize, maxNItems};

	if (!queue)
	{
		return -ENOMEM;
	}
	return write_result(ret, queue);
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  queue_delete(struct SObjStruct *heapCapability, void *queue)
{
	return deallocate<Queue>(heapCapability, queue);
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  queue_items_remaining(void *que, size_t *ret)
{
	if (!check_pointer<PermissionSet{Permission::Store}>(ret))
	{
		return -EINVAL;
	}
	return typed_op<Queue>(que, [&](Queue &queue) {
		*ret = queue.items_remaining();
		return 0;
	});
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  queue_send(Timeout *timeout, void *que, const void *src)
{
	return typed_op<Queue>(que, [&](Queue &queue) {
		if (!check_pointer<PermissionSet{Permission::Load}>(
		      src, queue.item_size()) ||
		    !check_timeout_pointer(timeout))
		{
			return std::pair{-EINVAL, false};
		}
		return queue.send(src, timeout);
	});
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  queue_recv(Timeout *timeout, void *que, void *dst)
{
	return typed_op<Queue>(que, [&](Queue &queue) {
		// TODO: We may need to sink this check down further because we may
		// also require store-capability if the message contains
		// capabilities.
		if (!check_pointer<PermissionSet{Permission::Store}>(
		      dst, queue.item_size()) ||
		    !check_timeout_pointer(timeout))
		{
			return std::pair{-EINVAL, false};
		}
		return queue.recv(dst, timeout);
	});
}

int __cheri_compartment("sched")
  semaphore_create(Timeout           *timeout,
                   struct SObjStruct *heapCapability,
                   void             **ret,
                   size_t             maxNItems)
{
	HeapObject<Queue> queue{timeout, heapCapability, nullptr, 0, maxNItems};
	if (!queue)
	{
		return -ENOMEM;
	}

	return write_result(ret, queue);
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  semaphore_delete(struct SObjStruct *heapCapability, void *sema)
{
	return deallocate<Queue>(heapCapability, sema);
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  semaphore_take(Timeout *timeout, void *sema)
{
	return typed_op<Queue>(sema, [&](Queue &queue) {
		if (!queue.is_semaphore())
		{
			return std::pair{-EINVAL, false};
		}
		return queue.send(nullptr, timeout);
	});
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  semaphore_give(Timeout *timeout, void *sema)
{
	return typed_op<Queue>(sema, [&](Queue &queue) {
		if (!queue.is_semaphore())
		{
			return std::pair{-EINVAL, false};
		}
		return queue.recv(nullptr, timeout);
	});
}

// thread APIs
SystickReturn __cheri_compartment("sched") thread_systemtick_get()
{
	uint64_t      ticks = Thread::ticksSinceBoot;
	uint32_t      hi    = ticks >> 32;
	uint32_t      lo    = ticks;
	SystickReturn ret   = {.lo = lo, .hi = hi};

	return ret;
}

int __cheri_compartment("sched") thread_sleep(Timeout *timeout)
{
	if (!check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	Thread::current_get()->suspend(timeout, nullptr, true);
	return 0;
}

uint16_t __cheri_compartment("sched") thread_id_get(void)
{
	return Thread::current_get()->id_get();
}

int __cheri_compartment("sched")
  event_create(Timeout *timeout, struct SObjStruct *heapCapability, void **ret)
{
	HeapObject<Event> event{timeout, heapCapability};

	if (!event)
	{
		return -ENOMEM;
	}

	return write_result(ret, event);
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  event_bits_wait(Timeout  *timeout,
                  void     *evt,
                  uint32_t *retBits,
                  uint32_t  bitsToWait,
                  bool      clearOnExit,
                  bool      waitAll)
{
	if (!check_pointer<PermissionSet{Permission::Store}>(retBits) ||
	    !check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return typed_op<Event>(evt, [&](Event &event) {
		return event.bits_wait(
		  retBits, bitsToWait, clearOnExit, waitAll, timeout);
	});
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  event_bits_set(void *evt, uint32_t *retBits, uint32_t bitsToSet)
{
	if (!check_pointer<PermissionSet{Permission::Store}>(retBits))
	{
		return -EINVAL;
	}
	return typed_op<Event>(
	  evt, [&](Event &event) { return event.bits_set(retBits, bitsToSet); });
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  event_bits_get(void *evt, uint32_t *retBits)
{
	if (!check_pointer<PermissionSet{Permission::Store}>(retBits))
	{
		return -EINVAL;
	}
	return typed_op<Event>(
	  evt, [&](Event &event) { return event.bits_get(retBits); });
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  event_bits_clear(void *evt, uint32_t *retBits, uint32_t bitsToClear)
{
	if (!check_pointer<PermissionSet{Permission::Store}>(retBits))
	{
		return -EINVAL;
	}
	return typed_op<Event>(evt, [&](Event &event) {
		return event.bits_clear(retBits, bitsToClear);
	});
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  event_delete(struct SObjStruct *heapCapability, void *evt)
{
	return deallocate<Event>(heapCapability, evt);
}

int futex_timed_wait(Timeout        *timeout,
                     const uint32_t *address,
                     uint32_t        expected)
{
	if (!check_timeout_pointer(timeout) ||
	    !check_pointer<PermissionSet{Permission::Load}>(address))
	{
		return -EINVAL;
	}
	// If the address does not contain the expected value then this call
	// raced with an update in another thread, return success immediately.
	if (*address != expected)
	{
		return 0;
	}
	Thread *currentThread = Thread::current_get();
	Debug::log("{} waiting on futex {} for {} ticks",
	           currentThread,
	           address,
	           timeout->remaining);
	currentThread->futexWaitAddress = Capability{address}.address();
	currentThread->suspend(timeout, &futexWaitingList);
	bool timedout                   = currentThread->futexWaitAddress == 0;
	currentThread->futexWaitAddress = 0;
	// If we woke up from a timer, report timeout.
	if (timedout)
	{
		return -ETIMEDOUT;
	}
	// If the memory for the futex was deallocated out from under us,
	// return an error.
	if (!Capability{address}.is_valid())
	{
		return -EINVAL;
	}
	Debug::log("{} woke after waiting on futex {}", currentThread, address);
	return 0;
}

int futex_wake(uint32_t *address, uint32_t count)
{
	if (!check_pointer<PermissionSet{Permission::Store}>(address))
	{
		return -EINVAL;
	}
	ptraddr_t key = Capability{address}.address();

	auto [shouldYield, woke] = futex_wake(key, count);

	if (shouldYield)
	{
		Thread::yield_interrupt_enabled();
	}

	return woke;
}

int multiwaiter_create(Timeout           *timeout,
                       struct SObjStruct *heapCapability,
                       ::MultiWaiter    **ret,
                       size_t             maxItems)
{
	int error;
	// Don't bother checking if timeout is valid, the allocator will check for
	// us.
	auto mw =
	  sched::MultiWaiter::create(timeout, heapCapability, maxItems, error);
	if (!mw)
	{
		return error;
	}

	return write_result(reinterpret_cast<void **>(ret), mw);
}

int multiwaiter_delete(struct SObjStruct *heapCapability, ::MultiWaiter *mw)
{
	return deallocate<sched::MultiWaiter>(heapCapability, mw);
}

int multiwaiter_wait(Timeout           *timeout,
                     ::MultiWaiter     *waiter,
                     EventWaiterSource *events,
                     size_t             newEventsCount)
{
	return typed_op<sched::MultiWaiter>(waiter, [&](sched::MultiWaiter &mw) {
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
			case sched::MultiWaiter::EventOperationResult::Error:
				Debug::log("Adding events returned error");
				return -EINVAL;
			case sched::MultiWaiter::EventOperationResult::Sleep:
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
			case sched::MultiWaiter::EventOperationResult::Wake:
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

uint16_t *thread_id_get_pointer(void)
{
	return sched::Thread::current_thread_id_pointer();
}

namespace
{
	/**
	 *
	 */
	struct InterruptCapability : Handle
	{
		/**
		 * Type marker used by `Handle`, tells it to use the dynamic path.
		 */
		static constexpr auto TypeMarker = Handle::Type::Dynamic;

		/**
		 * Dynamic type marker used by `Handle`.
		 */
		static Capability<void> dynamic_type_marker()
		{
			return STATIC_SEALING_TYPE(InterruptKey);
		}

		/**
		 * Padding for compatibility with the token layout.  This can go away
		 * at some point.
		 */
		uint32_t padding;

		/**
		 * The public structure state.
		 */
		InterruptCapabilityState state;
	};
} // namespace

[[cheri::interrupt_state(disabled)]] const uint32_t *
interrupt_futex_get(struct SObjStruct *sealed)
{
	auto     *interruptCapability = Handle::unseal<InterruptCapability>(sealed);
	uint32_t *result              = nullptr;
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

[[cheri::interrupt_state(disabled)]] int
interrupt_complete(struct SObjStruct *sealed)
{
	auto *interruptCapability = Handle::unseal<InterruptCapability>(sealed);
	if (interruptCapability && interruptCapability->state.mayComplete)
	{
		InterruptController::master().interrupt_complete(
		  interruptCapability->state.interruptNumber);
		return 0;
	}
	return -EPERM;
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

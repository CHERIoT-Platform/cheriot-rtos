// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "../switcher/tstack.h"
#include "multiwait.h"
#include "plic.h"
#include "thread.h"
#include "timer.h"
#include <cdefs.h>
#include <cheri.hh>
#include <compartment.h>
#include <futex.h>
#include <new>
#include <priv/riscv.h>
#include <riscvreg.h>
#include <simulator.h>
#include <stdint.h>
#include <stdlib.h>
#include <thread.h>

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
		Debug::Invariant(CHERI::Capability{info}.length() ==
		                   sizeof(*info) * CONFIG_THREADS_NUM,
		                 "Thread info is {} bytes, expected {} for {} threads",
		                 CHERI::Capability{info}.length(),
		                 sizeof(*info) * CONFIG_THREADS_NUM,
		                 CONFIG_THREADS_NUM);

		for (size_t i = 0; auto *threadSpace : threadSpaces)
		{
			Thread *th = new (threadSpace)
			  Thread(info[i].trustedStack, info[i].threadid, info[i].priority);
			th->ready(Thread::WakeReason::Timer);
			i++;
		}

#if DEVICE_EXISTS(plic)
		Plic::master_init();
#endif
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
		bool schedNeeded;

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
				schedNeeded = Plic::master().do_external_interrupt();
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

} // namespace sched

using namespace sched;

// queue APIs
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  queue_create(void **ret, size_t itemSize, size_t maxNItems)
{
	std::unique_ptr<char> storage{
	  static_cast<char *>(calloc(itemSize, maxNItems))};
	if (storage == nullptr)
	{
		return -ENOMEM;
	}

	auto queue = std::unique_ptr<Queue>{
	  new (std::nothrow) Queue(std::move(storage), itemSize, maxNItems)};

	if (!check_pointer<PermissionSet{Permission::Store,
	                                 Permission::LoadStoreCapability}>(ret))
	{
		return -EINVAL;
	}

	*ret = compart_seal(queue.release());

	return 0;
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  queue_delete(void *que)
{
	return typed_op<Queue>(que, [&](Queue &queue) {
		queue.~Queue();
		free(&queue);
		return 0;
	});
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
  queue_send(void *que, const void *src, Timeout *timeout)
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
  queue_recv(void *que, void *dst, Timeout *timeout)
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

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  semaphore_create(void **ret, size_t maxNItems)
{
	auto queue =
	  std::unique_ptr<Queue>{new (std::nothrow) Queue(nullptr, 0, maxNItems)};
	if (queue == nullptr)
	{
		return -ENOMEM;
	}

	if (!check_pointer<PermissionSet{Permission::Store,
	                                 Permission::LoadStoreCapability}>(ret))
	{
		return -EINVAL;
	}

	*ret = compart_seal(queue.release());

	return 0;
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  semaphore_delete(void *sema)
{
	return typed_op<Queue>(sema, [](Queue &queue) {
		queue.~Queue();
		free(&queue);
		return 0;
	});
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  semaphore_take(void *sema, Timeout *timeout)
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
  semaphore_give(void *sema, Timeout *timeout)
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

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  event_create(void **ret)
{
	auto event = std::unique_ptr<Event>{new (std::nothrow) Event()};

	if (event == nullptr)
	{
		return -ENOMEM;
	}

	if (!check_pointer<PermissionSet{Permission::Store,
	                                 Permission::LoadStoreCapability}>(ret))
	{
		return -EINVAL;
	}

	*ret = compart_seal(event.release());

	return 0;
}

[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  event_bits_wait(void     *evt,
                  uint32_t *retBits,
                  uint32_t  bitsToWait,
                  bool      clearOnExit,
                  bool      waitAll,
                  Timeout  *timeout)
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
  event_delete(void *evt)
{
	return typed_op<Event>(evt, [&](Event &event) {
		event.~Event();
		free(&event);
		return 0;
	});
}

[[cheri::interrupt_state(disabled)]] void *__cheri_compartment("sched")
  ethernet_event_get()
{
	return Plic::master().ethernet_event_get();
}

[[cheri::interrupt_state(disabled)]] void __cheri_compartment("sched")
  ethernet_intr_complete()
{
	Plic::master().ethernet_intr_complete();
}

namespace
{
	/**
	 * Priority-sorted list of threads waiting for a futex.
	 */
	Thread *futexWaitingList;

	/**
	 * Constant value used to represent an unbounded sleep.
	 */
	static constexpr auto UnboundedSleep = std::numeric_limits<uint32_t>::max();

} // namespace

int futex_timed_wait(uint32_t *address, uint32_t expected, Timeout *timeout)
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

	bool shouldYield = false;
	// The number of threads that we've woken, this is the return value on
	// success.
	int woke = 0;
	Thread::walk_thread_list(
	  futexWaitingList,
	  [&](Thread *thread) {
		  if (thread->futexWaitAddress == key)
		  {
			  shouldYield = thread->ready(Thread::WakeReason::Futex);
			  count--;
			  woke++;
		  }
	  },
	  [&]() { return count == 0; });

	if (count > 0)
	{
		auto multiwaitersWoken =
		  sched::MultiWaiter::wake_waiters(address, count);
		count -= multiwaitersWoken;
		woke += multiwaitersWoken;
		shouldYield |= (multiwaitersWoken > 0);
	}

	if (shouldYield)
	{
		Thread::yield_interrupt_enabled();
	}

	return woke;
}

int multiwaiter_create(::MultiWaiter **ret, size_t maxItems)
{
	int  error;
	auto mw = sched::MultiWaiter::create(maxItems, error);

	if (!check_pointer<PermissionSet{Permission::Store,
	                                 Permission::LoadStoreCapability}>(ret))
	{
		return -EINVAL;
	}
	if (mw)
	{
		*ret =
		  reinterpret_cast<::MultiWaiter *>(compart_seal(mw.release()).get());
		return 0;
	}
	return error;
}

int multiwaiter_delete(::MultiWaiter *mw)
{
	auto *unsealed = Handle::unseal<sched::MultiWaiter>(mw);
	if (unsealed != nullptr)
	{
		delete unsealed;
		return 0;
	}
	return -EINVAL;
}

int multiwaiter_wait(::MultiWaiter     *waiter,
                     EventWaiterSource *events,
                     size_t             newEventsCount,
                     Timeout           *timeout)
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

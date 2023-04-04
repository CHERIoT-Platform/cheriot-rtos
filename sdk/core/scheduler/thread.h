// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "common.h"
#include <cdefs.h>
#include <priv/riscv.h>
#include <strings.h>
#include <utils.h>

namespace sched
{
	/// The total number of thread priorities.
	constexpr uint16_t ThreadPrioNum = 32U;

	// Forward declaration of MultiWaiter so that we can use a pointer to it in
	// thread structures.
	class MultiWaiter;

	template<size_t NPrios>
	class ThreadImpl final : private utils::NoCopyNoMove
	{
		/**
		 * The number of threads that are currently running.  This is
		 * decremented when a thread exits.
		 *
		 * This count does not include the idle thread.
		 */
		inline static int threadCount = CONFIG_THREADS_NUM;

		public:
		struct EventBits
		{
			/// The set of event flags that this thread is waiting for.
			uint32_t bits : 24;
			/// Automatically clear the bits we waited on when woken up.
			bool clearOnExit : 1;
			uint32_t : 1;
			/// Wait until all the specified bits are set.
			bool waitForAll : 1;
			uint32_t : 5;
		};

		enum class ThreadState : uint8_t
		{
			Ready,
			Suspended,
			/// The thread has exited and should never be scheduled.
			Exited
		};
		enum class WakeReason : uint8_t
		{
			Timer,
			Queue,
			Event,
			Futex,
			/**
			 * Woken because one or more events registered with a multi-waiter
			 * object has occurred.
			 */
			MultiWaiter,
			/// Woken up because the data structure is gone.
			Delete
		};

		/**
		 * The number of timer ticks elapsed since boot. We use uint64_t and
		 * assume it never overflows. Note that uint64_t is not atomic on
		 * 32-bit platforms, so this field is always modified with interrupts
		 * disabled.
		 */
		static inline uint64_t ticksSinceBoot;
		/// All threads that are suspended must be on this list.
		static inline ThreadImpl *waitingList;

		/// Returns the current running thread.
		static ThreadImpl *current_get()
		{
			return current;
		}

		/**
		 * Run the scheduler to pick the next thread to run.  This returns a
		 * sealed capability to the trusted stack of the new thread, ready to be
		 * installed.
		 */
		static TrustedStack *schedule(TrustedStack *tstack)
		{
			ThreadImpl *th = current;

			if (th != nullptr)
			{
				if (th->state == ThreadState::Ready)
				{
					priorityList[th->priority] = th->next;
				}
				th->tStackPtr = tstack;
			}
			else
			{
				schedTStack = tstack;
			}

			current = priorityList[highestPriority];
			return current ? current->tStackPtr : schedTStack;
		}

		/**
		 * When yielding inside the scheduler compartment, we almost always want
		 * to re-enable interrupts before ecall. If we don't, then a thread with
		 * interrupt enabled can just call a scheduler function with a long
		 * timeout, essentially gaining the ability to indefinitely block
		 * interrupts. Worse, if this is the only thread, then it blocks
		 * interrupts forever for the whole system.
		 */
		static void yield_interrupt_enabled()
		{
			__asm volatile("ecall");
		}

		static uint32_t yield_timed()
		{
			uint64_t ticksAtStart = ticksSinceBoot;

			yield_interrupt_enabled();

			uint64_t elapsed = ticksSinceBoot - ticksAtStart;
			if (elapsed > std::numeric_limits<uint32_t>::max())
			{
				Debug::Assert(false, "Thread slept for too long, likely a bug");
				return std::numeric_limits<uint32_t>::max();
			}
			return elapsed;
		}

		/**
		 * Yield for up to the specified timeout.  May return early if the
		 * thread is on a sleep queue.  The caller should check permissions on
		 * the timeout parameter before the call, but it will be updated only
		 * if it is not freed by another thread during this call.
		 *
		 * Returns true on timeout, false otherwise.
		 */
		bool suspend(Timeout     *t,
		             ThreadImpl **newSleepQueue,
		             bool         yieldUnconditionally = false)
		{
			if (t->remaining != 0)
			{
				suspend(t->remaining, newSleepQueue);
			}
			if ((t->remaining != 0) || yieldUnconditionally)
			{
				auto elapsed = yield_timed();
				if (CHERI::Capability{t}.is_valid())
				{
					t->elapse(elapsed);
					if (t->remaining > 0)
					{
						return false;
					}
				}
			}
			return true;
		}

		/**
		 * The trusted stack for the idle thread. This trusted stack has a
		 * depth of 0, so we only use it for the register file and no
		 * compartment calls are allowed for the idle thread.
		 * This is also a sealed handle from the switcher, which can only be
		 * used for comparison.
		 */
		static inline TrustedStack *schedTStack;

		ThreadImpl(TrustedStack *tstack, uint16_t threadid, uint16_t priority)
		  : threadId(threadid),
		    priority(priority),
		    expiryTime(-1),
		    state(ThreadState::Suspended),
		    sleepQueue(nullptr),
		    tStackPtr(tstack)
		{
			// All threads are created in blocked state.
			timer_list_insert(&waitingList);
		}

		/**
		 * Ready this thread. Remove self from the timer list, and optionally,
		 * the list of the resource that this thread was blocked on. If thread
		 * is readied not by the resource it was blocked on but by a timeout or
		 * the resource disappearing, do some clean-ups.
		 * @param the reason why this thread is now able to be scheduled
		 */
		bool ready(WakeReason reason)
		{
			int64_t ticksLeft;
			bool    schedule = false;

			// We must be suspended.
			Debug::Assert(state == ThreadState::Suspended,
			              "Waking thread that is in state {}, not suspended",
			              state);
			// First, remove self from the timer waiting list.
			timer_list_remove(&waitingList);
			if (sleepQueue != nullptr)
			{
				// We were on a list waiting for some resource. Remove ourselves
				// from that list.
				list_remove(sleepQueue);
				sleepQueue = nullptr;
			}
			state = ThreadState::Ready;
			if (priorityList[priority] == nullptr)
			{
				// First thread at this priority ready.
				priorityMap |= (1U << priority);
				if (priority > highestPriority)
				{
					highestPriority = priority;
					schedule        = true;
				}
			}
			if (reason == WakeReason::Timer || reason == WakeReason::Delete)
			{
				multiWaiter = nullptr;
			}
			list_insert(&priorityList[priority]);

			return schedule;
		}

		/**
		 * Mutation-safe walker for a thread list.  The first argument is the
		 * list to walk, the second is the visitor that is invoked with each
		 * thread.  An optional third argument can be used to exit the loop
		 * early.  If this returns `true` then iteration will end before
		 * reaching the end of the loop.
		 *
		 * It is safe to call `ready` on threads visited by this function.
		 */
		template<typename Visitor,
		         typename TerminateCondition = decltype([]() { return false; })>
		__always_inline void static walk_thread_list(
		  ThreadImpl          *list,
		  Visitor            &&visitor,
		  TerminateCondition &&terminateCondition = TerminateCondition{})
		{
			for (auto *thread = list;
			     (thread != nullptr) && !terminateCondition();)
			{
				// Get the next pointer *before* any operation that might
				// remove this thread from the list.
				auto *next = thread->next;
				visitor(thread);
				// The last thread left in the list points to itself.
				thread = (thread == next) ? nullptr : thread = next;
			}
		}

		/**
		 * Suspend this thread. Take it off the ready list. If it is suspended
		 * waiting on a resource, add it to the list of that resource. No
		 * matter what, it has to be added to the timer list.
		 */
		void suspend(uint32_t waitTicks, ThreadImpl **newSleepQueue)
		{
			Debug::Assert(state == ThreadState::Ready,
			              "Suspending thread that is in state {}, not ready",
			              state);
			list_remove(&priorityList[priority]);
			state = ThreadState::Suspended;
			if (priorityList[priority] == nullptr)
			{
				// We just suspended the last ready thread at this priority.
				priorityMap &= ~(1U << priority);
				if (priorityMap == 0)
				{
					highestPriority = 0;
				}
				else
				{
					// clz is only for 32-bit.
					static_assert(NPrios == 32);
					uint16_t topZeroes = clz(priorityMap);

					highestPriority = NPrios - 1 - topZeroes;
				}
			}
			if (newSleepQueue != nullptr)
			{
				list_insert(newSleepQueue);
				sleepQueue = newSleepQueue;
			}
			expiryTime =
			  (waitTicks == UINT32_MAX ? -1 : ticksSinceBoot + waitTicks);

			timer_list_insert(&waitingList);
		}

		/**
		 * Cause the current thread to exit.  It will be removed from the
		 * scheduling queue.  The caller is responsible for invoking the
		 * scheduler to run another thread.
		 *
		 * Returns true if this was the last thread, false otherwise.
		 */
		static bool exit()
		{
			Debug::log("Thread exited, {} threads remaining", threadCount - 1);
			current->list_remove(&priorityList[current->priority]);
			current->state = ThreadState::Exited;
			return (--threadCount) == 0;
		}

		/**
		 * Insert self into a list of threads. headPtr can be nullptr if we are
		 * the first one on this list. The list is sorted by priority. Higher
		 * priority is at head, lower at tail.
		 */
		void list_insert(ThreadImpl **headPtr)
		{
			ThreadImpl *head = *headPtr;

			if (head == nullptr)
			{
				next = prev = *headPtr = this;
			}
			else
			{
				ThreadImpl *iter = head->prev;
				ThreadImpl *iterNext;

				// Go back from tail, and stop at the first Thread whose
				// Priority >= ours.
				while (iter->priority < priority)
				{
					iter = iter->prev;
					if (iter == head->prev)
					{
						break;
					}
				}
				iterNext   = iter->next;
				iter->next = iterNext->prev = this;
				next                        = iterNext;
				prev                        = iter;

				if (priority > head->priority)
				{
					*headPtr = this;
				}
			}
		}

		/// Same as list_insert(), but sorted on ExpiryTime.
		void timer_list_insert(ThreadImpl **headPtr)
		{
			ThreadImpl *head = *headPtr;

			Debug::Assert(state == ThreadState::Suspended,
			              "Inserting thread into timer list that is in state "
			              "{}, not suspended",
			              state);
			if (head == nullptr)
			{
				timerNext = timerPrev = *headPtr = this;
			}
			else
			{
				ThreadImpl *iter = head->timerPrev;
				ThreadImpl *iterNext;

				// Go back from tail, and stop at the first Thread whose expiry
				// <= ours.
				while (iter->expiryTime > expiryTime)
				{
					iter = iter->timerPrev;
					if (iter == head->timerPrev)
					{
						break;
					}
				}
				iterNext        = iter->timerNext;
				iter->timerNext = iterNext->timerPrev = this;
				timerNext                             = iterNext;
				timerPrev                             = iter;

				if (expiryTime < head->expiryTime)
				{
					*headPtr = this;
				}
			}
		}

		/// Remove self from the list headPtr points to.
		void list_remove(ThreadImpl **headPtr)
		{
			ThreadImpl *head = *headPtr;

			if (next == this)
			{
				// We are the only one left on this list, clear the head.
				Debug::Assert(prev == this,
				              "Invalid loop in thread list.  Next pointer "
				              "points to this ({}) but previous pointer is {}",
				              this,
				              prev);
				*headPtr = nullptr;
			}
			else
			{
				ThreadImpl *prevThread = prev;
				ThreadImpl *nextThread = next;

				prevThread->next = nextThread;
				nextThread->prev = prevThread;

				if (head == this)
				{
					*headPtr = next;
				}
			}
			next = prev = nullptr;
		}

		/// Same as list_remove(), but for the timer list.
		void timer_list_remove(ThreadImpl **headPtr)
		{
			ThreadImpl *head = *headPtr;

			if (timerNext == this)
			{
				// We are the only one left on this list, clear the head.
				Debug::Assert(timerPrev == this,
				              "Invalid loop in thread list.  Next pointer "
				              "points to this ({}) but previous pointer is {}",
				              this,
				              timerPrev);
				*headPtr = nullptr;
			}
			else
			{
				ThreadImpl *prevThread = timerPrev;
				ThreadImpl *nextThread = timerNext;

				prevThread->timerNext = nextThread;
				nextThread->timerPrev = prevThread;

				if (head == this)
				{
					*headPtr = timerNext;
				}
			}
			timerNext = timerPrev = nullptr;
		}

		uint16_t id_get()
		{
			return threadId;
		}

		uint16_t priority_get()
		{
			return priority;
		}

		~ThreadImpl()
		{
			// We have static definition of threads. We only create threads in
			// the boot loader and never destroy threads.
			panic();
		}

		/// Linked list fields. They are nullptr, unless blocked on a resource,
		/// which then get linked to the list of that resource.
		///@{
		ThreadImpl *prev;
		ThreadImpl *next;
		///@}
		/// Linked list fields to the global timer list, when this thread is
		/// blocked with a timeout.
		///@{
		ThreadImpl *timerPrev;
		ThreadImpl *timerNext;
		///@}
		/// Pointer to the list of the resource this thread is blocked on.
		ThreadImpl **sleepQueue;
		/// If suspended, when will this thread expire. The maximum value is
		/// special-cased to mean blocked indefinitely.
		uint64_t expiryTime;

		/// The number of cycles that this thread has been scheduled for.
		uint64_t cycles;

		/// The number of cycles accounted to the idle thread.
		static inline uint64_t idleThreadCycles;

		union
		{
			/// When the thread wakes up from event group, this stores the bits
			/// right after waking up (but before clearing the bits if
			/// clearOnExit is set). 0 if it's woken up due to timer expiry.
			EventBits eventWaitBits;
			/**
			 * If this thread is blocked on a futex, this holds the address of
			 * the futex.  This is set to 0 if woken via timeout.
			 */
			ptraddr_t futexWaitAddress;
			/**
			 * If this thread is blocked on a multiwaiter, this holds the
			 * address of the multiwaiter object.
			 */
			MultiWaiter *multiWaiter;
		};
		TrustedStack *tStackPtr;

		private:
		/// the current runnning thread
		static inline ThreadImpl *current;
		/// NPrios number of lists, each linking the threads of this priority
		static inline ThreadImpl *priorityList[NPrios];
		/// A bit field indicating the presence of ready threads. A set bit at
		/// bit index N means there's at least one thread ready with priority N.
		static inline uint32_t priorityMap;
		/// the highest priority of all the current threads that are ready
		static inline uint16_t highestPriority;

		uint16_t    threadId;
		uint16_t    priority;
		ThreadState state;
	};

	using Thread = ThreadImpl<ThreadPrioNum>;

} // namespace sched

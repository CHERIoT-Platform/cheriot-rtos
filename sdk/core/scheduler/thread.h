// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "common.h"
#include <cdefs.h>
#include <platform-timer.hh>
#include <priv/riscv.h>
#include <strings.h>
#include <tick_macros.h>
#include <utils.hh>

// Forward declaration of MultiWaiter so that we can use a pointer to it in
// thread structures.
class MultiWaiterInternal;

namespace
{
	/// The total number of thread priorities.
	constexpr uint16_t ThreadPrioNum = 32U;

	uint64_t expiry_time_for_timeout(uint32_t timeout);

	template<size_t NPrios>
	class ThreadImpl final : private utils::NoCopyNoMove
	{
		/**
		 * The number of threads that are currently running.  This is
		 * decremented when a thread exits.
		 *
		 * This count does not include the idle thread.
		 */
		inline static uint16_t threadCount = CONFIG_THREADS_NUM;

		static_assert(CONFIG_THREADS_NUM <
		              std::numeric_limits<decltype(threadCount)>::max());

		public:
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
		static CHERI_SEALED(TrustedStack *)
		  schedule(CHERI_SEALED(TrustedStack *) tstack)
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
			if (current)
			{
				Debug::Assert(highestPriority == current->priority,
				              "Thread {} is on the run queue for priority {}, "
				              "but is currently priority {} (originally {})",
				              current->threadId,
				              highestPriority,
				              current->priority,
				              current->OriginalPriority);
				if (current->priority != current->OriginalPriority)
				{
					Debug::log(
					  "Running thread {} with boosted priority ({} from {})",
					  current->id_get(),
					  current->priority,
					  current->OriginalPriority);
				}
				if (current != th)
				{
					current->expiryTime = TimerCore::time();
				}
#ifdef CLANG_TIDY
				// The static analyser thinks that `debug_log_message_write` may
				// assign `nullptr` to `current`.
				__builtin_assume(current != nullptr);
#endif
				return current->tStackPtr;
			}
			return schedTStack;
		}

		/**
		 * Returns true if any thread is ready to run.
		 */
		static bool any_ready()
		{
			return priorityMap != 0;
		}

		static uint32_t yield_timed()
		{
			uint64_t ticksAtStart = ticksSinceBoot;

			yield();

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
		             bool         yieldUnconditionally = false,
		             bool         yieldNotSleep        = false)
		{
			if (t->remaining != 0)
			{
				suspend(t->remaining, newSleepQueue, yieldNotSleep);
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
		static inline CHERI_SEALED(TrustedStack *) schedTStack;

		ThreadImpl(CHERI_SEALED(TrustedStack *) tstack,
		           uint16_t threadid,
		           uint16_t priority)
		  : threadId(threadid),
		    priority(priority),
		    OriginalPriority(priority),

		    state(ThreadState::Suspended),

		    sleepQueue(nullptr),
		    tStackPtr(tstack)
		{
			static_assert(NPrios <
			              std::numeric_limits<decltype(priority)>::max());
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
		void ready(WakeReason reason)
		{
			int64_t ticksLeft;

			// We must be suspended.
			Debug::Assert(state == ThreadState::Suspended,
			              "Waking thread that is in state {}, not suspended",
			              static_cast<ThreadState>(state));
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
				}
			}
			if (reason == WakeReason::Timer || reason == WakeReason::Delete)
			{
				multiWaiter = nullptr;
			}
			list_insert(&priorityList[priority]);
			isYielding = false;
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
		  ThreadImpl         *&list,
		  Visitor            &&visitor,
		  TerminateCondition &&terminateCondition = TerminateCondition{})
		{
			for (auto *thread = list;
			     (thread != nullptr) && !terminateCondition();)
			{
				// Get the next pointer *before* any operation that might
				// remove this thread from the list.
				auto *next = thread->next;
				// The thread queues are actually circularly linked lists to
				// avoid a head and tail pointer for each queue, so we spot the
				// last thread if it's the end.
				next = (list == next) ? nullptr : next;
				visitor(thread);
				thread = next;
			}
		}

		/**
		 * Suspend this thread. Take it off the ready list. If it is suspended
		 * waiting on a resource, add it to the list of that resource. No
		 * matter what, it has to be added to the timer list.
		 */
		void suspend(uint32_t     waitTicks,
		             ThreadImpl **newSleepQueue,
		             bool         yieldNotSleep = false)
		{
			isYielding = yieldNotSleep;
			Debug::Assert(state == ThreadState::Ready,
			              "Suspending thread that is in state {}, not ready",
			              static_cast<ThreadState>(state));
			list_remove(&priorityList[priority]);
			state = ThreadState::Suspended;
			priority_map_remove();
			if (newSleepQueue != nullptr)
			{
				list_insert(newSleepQueue);
				sleepQueue = newSleepQueue;
			}
			expiryTime = expiry_time_for_timeout(waitTicks);

			timer_list_insert(&waitingList);
		}

		/**
		 * Boost the thread's thread to `newPriority` if that is larger than
		 * the original priority or reset to the original priority if not.
		 */
		void priority_boost(uint8_t newPriority)
		{
			newPriority = std::max(newPriority, OriginalPriority);
			if (newPriority == priority)
			{
				return;
			}
			// If this thread is currently runnable, move it to the right run
			// queue.
			if (state == ThreadState::Ready)
			{
				list_remove(&priorityList[priority]);
				list_insert(&priorityList[newPriority]);
				priorityMap |= 1U << newPriority;
				priority_map_remove();
			}
			// Sleep queues are sorted by priority.  If we're on a sleep queue,
			// remove ourself and add ourself back in the right place.
			if (sleepQueue != nullptr)
			{
				list_remove(sleepQueue);
			}
			priority = newPriority;
			if (sleepQueue != nullptr)
			{
				list_insert(sleepQueue);
			}
		}

		/**
		 * Returns true if this thread is running with the highest priority of
		 * any runnable threads.
		 */
		bool is_highest_priority()
		{
			Debug::Assert(priority <= highestPriority,
			              "Priority ({}) should not be higher than the highest "
			              "priority ({})!",
			              priority,
			              highestPriority);
			return priority == highestPriority;
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
			current->priority_map_remove();
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
			              static_cast<ThreadState>(state));
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

		uint8_t priority_get()
		{
			return priority;
		}

		bool is_ready()
		{
			return state == ThreadState::Ready;
		}

		bool is_yielding()
		{
			return isYielding;
		}

		/**
		 * Returns true if there are other runnable threads with the same
		 * priority as this thread.
		 */
		bool has_priority_peers()
		{
			Debug::Assert(state == ThreadState::Ready,
			              "Checking for peers on thread that is in state {}, "
			              "not ready",
			              static_cast<ThreadState>(state));
			return next != this;
		}

		/**
		 * Returns true if the thread has run for a complete tick.  This must
		 * be called only on the currently running thread.
		 */
		bool has_run_for_full_tick()
		{
			Debug::Assert(this == current,
			              "Only the current thread is running");
			return TimerCore::time() >= expiryTime + TIMERCYCLES_PER_TICK;
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
		/**
		 * If suspended, when will this thread expire. The maximum value is
		 * special-cased to mean blocked indefinitely.
		 *
		 * When a thread is running, this the time at which it was scheduled.
		 */
		uint64_t expiryTime{static_cast<uint64_t>(-1)};

		/// The number of cycles that this thread has been scheduled for.
		uint64_t cycles;

		/// The number of cycles accounted to the idle thread.
		static inline uint64_t idleThreadCycles;

		union
		{
			/// State associated with waiting on a futex.
			struct
			{
				/**
				 * If this thread is blocked on a futex, this holds the address
				 * of the futex.  This is set to 0 if woken via timeout.
				 */
				ptraddr_t futexWaitAddress;
				/**
				 * The thread that we're priority boosting.  This is ignored if
				 * `futexPriorityInheriting` is false.
				 */
				uint16_t futexPriorityBoostedThread : 16;
				/**
				 * If this thread is waiting on a futex, should it be priority
				 * boosting the current holder of the futex?
				 */
				bool futexPriorityInheriting : 1;
			};
			/**
			 * If this thread is blocked on a multiwaiter, this holds the
			 * address of the multiwaiter object.
			 */
			MultiWaiterInternal *multiWaiter;
		};

		/**
		 * Sealed pointer to this thread's trusted stack and register-save area.
		 */
		CHERI_SEALED(TrustedStack *) tStackPtr;

		private:
		/**
		 * Helper to remove a thread from the priority map and update the
		 * highest priority, if it was the last runnable thread at that
		 * priority level.
		 */
		void priority_map_remove()
		{
			if (priorityList[priority] == nullptr)
			{
				// We just removed the last ready thread at this priority.
				priorityMap &= ~(1U << priority);
				if (priorityMap == 0)
				{
					highestPriority = 0;
				}
				else
				{
					// clz is only for 32-bit.
					static_assert(NPrios <= 32);
					uint_fast16_t topZeroes = clz(priorityMap);

					highestPriority = NPrios - 1 - topZeroes;
				}
			}
		}

		/// the current runnning thread
		static inline ThreadImpl *current;
		/// NPrios number of lists, each linking the threads of this priority
		static inline ThreadImpl *priorityList[NPrios];
		/// A bit field indicating the presence of ready threads. A set bit at
		/// bit index N means there's at least one thread ready with priority N.
		static inline uint32_t priorityMap;
		/// the highest priority of all the current threads that are ready
		static inline uint16_t highestPriority;

		uint16_t threadId;
		/**
		 * The current priority level for this thread.  This may be influenced
		 * by priority inheritance.
		 */
		uint8_t priority;
		/// The original priority level for this thread.  This never changes.
		const uint8_t OriginalPriority;
		ThreadState   state : 2;
		/**
		 * If the thread is yielding, it may be scheduled before its timeout
		 * expires, as long as no other threads are runnable or sleeping with
		 * shorter timeouts.
		 */
		bool isYielding : 1 {false};
	};

	using Thread = ThreadImpl<ThreadPrioNum>;

} // namespace

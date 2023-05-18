// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "multiwait.h"
#include "thread.h"
#include "timer.h"
#include <cdefs.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <utility>
#include <utils.hh>

namespace sched
{
	class Queue final : private utils::NoCopyNoMove, public Handle
	{
		/// The address is used as the send offset.
		HeapBuffer storage;
		/**
		 * The address to the recv location. We don't have a SendAddr because
		 * it's already captured in the address field of Storage.
		 */
		size_t recvAddr;
		/// size of each queue message
		const size_t ItemSize;
		/// max number of messages of this queue
		size_t maxNItems;
		/// current number of items in the queue
		size_t nItems;
		/// linked list containing all senders blocked on this queue
		Thread *sendWaitList;
		/// linked list containing all receivers blocked on this queue
		Thread *recvWaitList;

		public:
		/**
		 * Type marker used for `Handle::unseal_as`.
		 */
		static constexpr auto TypeMarker = Handle::Type::Queue;

		/**
		 * Unseal this as a queue, returning nullptr if it is not a sealed
		 * queue.
		 */
		Queue *unseal()
		{
			return unseal_as<Queue>();
		}

		/**
		 * Initialise a queue object. When `storage` is nullptr and `itemSize`
		 * is 0, this queue is used as a semaphore.
		 * @param storage memory allocated to store messages in queue
		 * @param itemSize size of each queue item
		 * @param maxNItems maximum number of items allowed in queue
		 */
		Queue(HeapBuffer &&messageStorage, size_t itemSize, size_t maxNItems)
		  : Handle(TypeMarker),
		    storage(std::move(messageStorage)),
		    recvAddr(CHERI::Capability(storage.get()).base()),
		    ItemSize(itemSize),
		    maxNItems(maxNItems),
		    nItems(0),
		    sendWaitList(nullptr),
		    recvWaitList(nullptr)
		{
			Debug::Assert(
			  (!storage && itemSize == 0) ||
			    (storage && (itemSize > 0) && (maxNItems > 0)),
			  "Invalid queue constructor arguments.  Storage: {}, itemSize: {}",
			  storage.get(),
			  itemSize);
		}

		Queue(std::nullptr_t, size_t itemSize, size_t maxNItems)
		  : Handle(TypeMarker),
		    storage(),
		    recvAddr(0),
		    ItemSize(itemSize),
		    maxNItems(maxNItems),
		    nItems(0),
		    sendWaitList(nullptr),
		    recvWaitList(nullptr)
		{
			Debug::Assert(itemSize == 0,
			              "Invalid queue constructor arguments. Storage: "
			              "nullptr, itemSize: {}",
			              itemSize);
		}

		/**
		 * Returns the size of an item.  Send and receive calls expect to be
		 * able to read or write this much data.
		 */
		size_t item_size()
		{
			return ItemSize;
		}

		/// Returns true if the queue is full, false otherwise.
		bool is_full()
		{
			return nItems == maxNItems;
		}

		/// Returns true if the queue is empty, false otherwise.
		bool is_empty()
		{
			return nItems == 0;
		}

		[[nodiscard]] __always_inline bool is_semaphore() const
		{
			return ItemSize == 0;
		}

		std::pair<int, bool> send(const void *src, Timeout *timeout)
		{
			Thread *curr = Thread::current_get();

			// No room for a new message
			if (is_full())
			{
				if (!timeout->may_block())
				{
					return {-EWOULDBLOCK, false};
				}

				ExceptionGuard::assert_safe_to_block();
				do
				{
					if (curr->suspend(timeout, &sendWaitList))
					{
						return {-ETIMEDOUT, false};
					}
					/*
					 * Waking up here doesn't necessarily mean there's space
					 * for our message. A higher-priority thread could have
					 * already filled the vacancy, so we have to double
					 * check there's really room for us.
					 *
					 * It is also possible that the queue object itself is freed
					 * while we were sleeping. In this case the while() check
					 * fails on a tag exception, and the default unwind
					 * behaviour is actually what we want.
					 */
				} while (nItems == maxNItems);
			}

			Debug::Assert(
			  nItems < maxNItems,
			  "Adding an item to a full queue (contains {} items, max: {})",
			  nItems,
			  maxNItems);
			// If we arrived here, then NItems must < MaxNItems and we must be
			// on the ready list.
			if (!is_semaphore())
			{
				// The clang analyser raises a false positive here for the
				// semaphore calls because it does not propagate the fact that
				// the caller has already checked `is_semaphore` and had it
				// return `true`.
				// Also, we want this memcpy() to finish before any internal
				// state of this queue is touched, because it may fault (storing
				// a local capability from the user to the storage) and we want
				// a clean force-unwind.
				memcpy(storage.get(), src, ItemSize); // NOLINT
				auto      storageCap     = CHERI::Capability{storage.get()};
				ptraddr_t storageAddress = storageCap.address() + ItemSize;
				ptraddr_t storageBase    = storageCap.base();
				/*
				 * Ring buffer wrap around.
				 * N.B. Never check against the top of `Storage` here. `Storage`
				 * may be a capability with larger length than the requested
				 * storage space because of imprecision or malloc alignment.
				 * Always check against the real intended storage space.
				 */
				if (storageAddress >= storageBase + maxNItems * ItemSize)
				{
					storageAddress = storageBase;
				}
				storage.set_address(storageAddress);
			}
			nItems++;

			bool shouldYield =
			  waitlist_unblock_one(recvWaitList, Thread::WakeReason::Queue);

			return {0, shouldYield};
		}

		std::pair<int, bool> recv(void *dst, Timeout *timeout)
		{
			Thread *curr = Thread::current_get();

			if (is_empty())
			{
				if (!timeout->may_block())
				{
					// Semaphore (ItemSize == 0) give cannot enter here. Nobody
					// has taken it.
					Debug::Assert(
					  ItemSize > 0, "Item size {} should be > 0", ItemSize);
					return {-EWOULDBLOCK, false};
				}

				ExceptionGuard::assert_safe_to_block();
				do
				{
					if (curr->suspend(timeout, &recvWaitList))
					{
						return {-ETIMEDOUT, false};
					}
					// Same as send(), we could force unwind here.
				} while (nItems == 0);
			}

			Debug::Assert(nItems > 0 && nItems <= maxNItems,
			              "Number of items {} in queue is out of range 0-{}",
			              nItems,
			              maxNItems);
			// If we arrived here, then NItems must > 0 and we must be on the
			// ready list.
			if (!is_semaphore())
			{
				CHERI::Capability newStorage{storage.get()};
				newStorage.address() = recvAddr;
				// The clang analyser raises a false positive here for the
				// semaphore calls because it does not propagate the fact that
				// the caller has already checked `is_semaphore` and had it
				// return `true`.
				// Similar to send(), make sure no internal state is changed
				// before changing internal state, for a clean force-unwind.
				memcpy(dst, newStorage.get(), ItemSize); // NOLINT
				recvAddr += ItemSize;
				ptraddr_t storageBase = CHERI::Capability{storage.get()}.base();
				if (recvAddr >= storageBase + ItemSize * maxNItems)
				{
					recvAddr = storageBase;
				}
			}
			nItems--;

			bool shouldYield =
			  waitlist_unblock_one(sendWaitList, Thread::WakeReason::Queue);

			return {0, shouldYield};
		}

		size_t items_remaining()
		{
			return nItems;
		}

		~Queue()
		{
			while (recvWaitList)
			{
				waitlist_unblock_one(recvWaitList, Thread::WakeReason::Delete);
			}
			while (sendWaitList)
			{
				waitlist_unblock_one(sendWaitList, Thread::WakeReason::Delete);
			}
		}

		private:
		/**
		 * Unblock one thread from a send or recv waiting list.
		 * @return true if rescheduling is needed
		 */
		bool waitlist_unblock_one(Thread *head, Thread::WakeReason reason)
		{
			Debug::Assert((head == nullptr) || (*head->sleepQueue == head),
			              "Item is on the wrong wait list ({}, expected {}",
			              (head != nullptr) ? *head->sleepQueue : nullptr,
			              head);
			auto wokeMultiwaiters = sched::MultiWaiter::wake_waiters(this);
			return ((head != nullptr) && head->ready(reason)) ||
			       wokeMultiwaiters;
		}
	};

	inline bool EventWaiter::trigger(Queue *queue)
	{
		if ((kind != EventWaiterQueue) || (Capability{queue} != eventSource))
		{
			return false;
		}
		if ((flags & EventWaiterQueueSendReady) && !queue->is_full())
		{
			set_ready(EventWaiterQueueSendReady);
		}
		if ((flags & EventWaiterQueueReceiveReady) && !queue->is_empty())
		{
			set_ready(EventWaiterQueueReceiveReady);
		}
		return readyEvents != 0;
	}

	inline bool EventWaiter::reset(Queue *queue, uint32_t conditions)
	{
		eventSource = queue;
		eventValue  = 0;
		flags       = conditions;
		kind        = EventWaiterQueue;
		readyEvents = 0;
		return trigger(queue);
	}
} // namespace sched

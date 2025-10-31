#include <cheri.hh>
#include <cstdlib>
#include <errno.h>
#include <locks.hh>
#include <queue.h>
#include <stddef.h>
#include <stdint.h>
#include <timeout.h>
#include <type_traits>
#include <unwind.h>

using namespace CHERI;
using cheriot::atomic;

using Debug = ConditionalDebug<false, "MessageQueue library">;

#ifdef __cplusplus
using MessageQueueCounter = atomic<uint32_t>;
#else
typedef uint32_t MessageQueueCounter;
#endif

#include <assembly-helpers.h>

namespace
{
	/**
	 * Helpers for the queue.  The queue uses two counters that wrap on double
	 * the number of elements.  This ensures that full and empty conditions
	 * have different values: a queue is full when the producer is one queue
	 * length ahead of the consumer, and empty when the producer and consumer
	 * are equal.
	 */

	/**
	 * Helper for wrapping increment.  Increments `counter`, wrapping to zero
	 * if it reaches double `size`.
	 */
	constexpr uint32_t increment_and_wrap(uint32_t size, uint32_t counter)
	{
		counter++;
		if (2 * size == counter)
		{
			return 0;
		}
		return counter;
	}

	/**
	 * Helper for wrapping add.  Adds `addend` to `counter`, wrapping to zero
	 * if it reaches double `size`.
	 */
	constexpr uint32_t
	add_and_wrap(uint32_t size, uint32_t counter, uint32_t addend)
	{
		counter += addend;
		if (counter >= 2 * size)
		{
			counter -= (2 * size);
		}
		return counter;
	}

	/**
	 * Returns the number of items in the queue for the given size with the
	 * specified producer and consumer counters.
	 */
	constexpr uint32_t
	items_remaining(uint32_t size, uint32_t producer, uint32_t consumer)
	{
		// If the consumer is ahead of the producer then the producer has
		// wrapped.  In this case, treat the consumer as a negative offset
		if (consumer > producer)
		{
			return (2 * size) - consumer + producer;
		}
		return producer - consumer;
	}

	/**
	 * Returns true if and only if the `size`, `producer`, and `consumer`
	 * counters indicate a full queue.
	 */
	constexpr bool is_full(uint32_t size, uint32_t producer, uint32_t consumer)
	{
		return items_remaining(size, producer, consumer) == size;
	}

	/**
	 * Returns true if and only if the `producer`, and `consumer` counters
	 * indicate an empty queue.
	 */
	constexpr bool is_empty(uint32_t producer, uint32_t consumer)
	{
		return producer == consumer;
	}

	/**
	 * Helper that exhaustively checks the correctness of the is-full
	 * calculation for a size.
	 *
	 * This is purely constexpr logic for compile-time checks, it generates no
	 * code.
	 */
	template<uint32_t Size>
	struct CheckIsFull
	{
		/**
		 * Check that the template arguments return false for is-full.  This is
		 * a separate template so that we see the values in a compiler error.
		 */
		template<uint32_t Producer, uint32_t Consumer>
		static constexpr void check_not_full()
		{
			static_assert(!is_full(Size, Producer, Consumer),
			              "is-full calculation is incorrect");
		}

		/**
		 * Check that the template arguments return true for is-full.  This is a
		 * separate template so that we see the values in a compiler error.
		 */
		template<uint32_t Producer, uint32_t Consumer>
		static constexpr void check_full()
		{
			static_assert(is_full(Size, Producer, Consumer),
			              "is-full calculation is incorrect");
		}

		/**
		 * Helper that uses `increment_and_wrap` to add `displacement` to
		 * `start`, giving the value of a counter after `displacement`
		 * increments.
		 */
		static constexpr uint32_t add(uint32_t start, uint32_t displacement)
		{
			for (uint32_t i = 0; i < displacement; i++)
			{
				start = increment_and_wrap(Size, start);
			}
			return start;
		}

		/**
		 * Check that items-remaining returns the correct value for the given
		 * counter values.
		 */
		template<uint32_t Producer, uint32_t Consumer, uint32_t Displacement>
		static constexpr void check_items_remaining()
		{
			static_assert(items_remaining(Size, Producer, Consumer) ==
			                Displacement,
			              "items-remaining calculation is incorrect");
		}

		template<auto A, auto B>
		static void constexpr check_equal()
		{
			static_assert(A == B);
		}

		template<uint32_t Expected, uint32_t Counter, uint32_t Addend>
		static constexpr void check_add_and_wrap()
		{
			if constexpr (Counter < 2 * Size)
			{
				check_equal<add_and_wrap(Size, Counter, Addend), Expected>();
			}
		}

		/**
		 * For every producer counter value from 0 up to `Size` after a consumer
		 * counter value, check that it returns the correct value for the
		 * items-remaining, is-empty, and is-full calculations.
		 */
		template<uint32_t Consumer, uint32_t Displacement = 0>
		static constexpr void check_offsets()
		{
			constexpr auto Producer = add(Consumer, Displacement);
			check_items_remaining<Producer, Consumer, Displacement>();
			check_add_and_wrap<Producer, Consumer, Displacement>();
			if constexpr (Displacement == 0)
			{
				static_assert(is_empty(Producer, Consumer),
				              "is-empty calculation is incorrect");
			}
			else
			{
				static_assert(!is_empty(Producer, Consumer),
				              "is-empty calculation is incorrect");
			}
			if constexpr (Displacement == Size)
			{
				check_full<Producer, Consumer>();
			}
			else
			{
				static_assert(Displacement < Size,
				              "Displacement overflowed somehow");
				check_not_full<Producer, Consumer>();
				check_offsets<Consumer, Displacement + 1>();
			}
		}

		/**
		 * Check every valid consumer value for the given size, and every valid
		 * producer value for each consumer value.
		 */
		template<uint32_t Consumer = 0>
		static constexpr bool check_sizes()
		{
			check_offsets<Consumer>();
			if constexpr (Consumer < Size * 2)
			{
				check_sizes<Consumer + 1>();
			}
			return true;
		}

		static constexpr bool Value = check_sizes();
	};

	/**
	 * Helper.  This is never false, it is always either true or compile
	 * failure.
	 */
	template<uint32_t Size>
	constexpr bool CheckIsFullValue = CheckIsFull<Size>::Value;

	// Check some sizes that are likely to be wrong (powers of two, primes, and
	// some other values)
	static_assert(CheckIsFullValue<1>, "CheckIsFull failed");
	static_assert(CheckIsFullValue<3>, "CheckIsFull failed");
	static_assert(CheckIsFullValue<4>, "CheckIsFull failed");
	static_assert(CheckIsFullValue<10>, "CheckIsFull failed");
	static_assert(CheckIsFullValue<17>, "CheckIsFull failed");
	static_assert(CheckIsFullValue<32>, "CheckIsFull failed");
	static_assert(CheckIsFullValue<33>, "CheckIsFull failed");

	/**
	 * Returns the index of the element in the queue indicated by `counter`.
	 */
	size_t index_at_counter(struct MessageQueue &handle, uint32_t counter)
	{
		// Handle wrap for the second run around the counter.
		return counter >= handle.queueSize ? counter - handle.queueSize
		                                   : counter;
	}

	/**
	 * Returns a pointer into the queue buffer starting at index.
	 */
	void *queue_pointer_at_index(struct MessageQueue &handle, size_t index)
	{
		Capability<void> pointer{&handle};
		pointer.address() +=
		  sizeof(MessageQueue) + (index * handle.elementSize);
		return pointer;
	}

	/**
	 * Flag lock that uses the two high bits of a word for the lock.
	 *
	 * The remaining bits are free for other uses, but should not be modified
	 * while the lock is not held.
	 */
	struct HighBitFlagLock
	{
		/**
		 * A reference to the word that holds the lock.
		 */
		atomic<uint32_t> &lockWord;

		/**
		 * The bit to use for the lock.
		 */
		static constexpr uint32_t LockBit                 = 1U << 31;
		static constexpr uint32_t WaitersBit              = 1U << 30;
		static constexpr uint32_t LockedInDestructModeBit = 1U << 29;

		// Function required to conform to the Lock concept.
		void lock()
		{
			__builtin_unreachable();
		}

		static constexpr uint32_t reserved_bits()
		{
			return LockBit | WaitersBit | LockedInDestructModeBit;
		}

		/**
		 * Try to acquire the lock.  Returns true on success, false on failure.
		 */
		bool try_lock(Timeout *t)
		{
			uint32_t value;
			do
			{
				value = lockWord.load();
				if ((value & LockedInDestructModeBit) != 0)
				{
					return false;
				}
				if (value & LockBit)
				{
					// If the lock is held, set the flag that indicates that
					// there are waiters.
					if ((value & WaitersBit) == 0)
					{
						if (!lockWord.compare_exchange_strong(
						      value, value | WaitersBit))
						{
							continue;
						}
					}
					if (lockWord.wait(t, value) == -ETIMEDOUT)
					{
						return false;
					}
					continue;
				}
			} while (
			  !lockWord.compare_exchange_strong(value, (value | LockBit)));
			return true;
		}

		/**
		 * Release the lock.
		 */
		void unlock()
		{
			uint32_t value;
			// Clear the lock bit.
			value = lockWord.load();
			// If we're releasing the lock with waiters, wake them up.
			if (lockWord.exchange(value & ~LockBit) & WaitersBit)
			{
				lockWord.notify_all();
			}
		}

		/**
		 * Set the lock in destruction mode. This has the same
		 * semantics as `flaglock_upgrade_for_destruction`.
		 */
		void upgrade_for_destruction()
		{
			// Atomically set the destruction bit.
			lockWord |= LockedInDestructModeBit;
			if (lockWord & WaitersBit)
			{
				lockWord.notify_all();
			}
		}
	};

	uint32_t counter_load(std::atomic<uint32_t> *counter)
	{
		return counter->load() & ~(HighBitFlagLock::reserved_bits());
	}

	void counter_store(std::atomic<uint32_t> *counter, uint32_t value)
	{
		uint32_t old;
		do
		{
			old = counter->load();
		} while (!counter->compare_exchange_strong(
		  old, (old & HighBitFlagLock::reserved_bits()) | value));
	}

} // namespace

int queue_destroy(AllocatorCapability  heapCapability,
                  struct MessageQueue *handle)
{
	int ret = 0;
	// Only upgrade the locks for destruction if we know that we will be
	// able to free the queue at the end. This will fail if passed a
	// restricted buffer, which will happen if `queue_destroy` is called on
	// a restricted queue.
	if (ret = heap_can_free(heapCapability, handle); ret != 0)
	{
		return ret;
	}

	HighBitFlagLock producerLock{handle->producer};
	producerLock.upgrade_for_destruction();

	HighBitFlagLock consumerLock{handle->consumer};
	consumerLock.upgrade_for_destruction();

	// This should not fail because of the `heap_can_free` check, unless we
	// run out of stack.
	if (ret = heap_free(heapCapability, handle); ret != 0)
	{
		return ret;
	}

	return ret;
}

ssize_t queue_allocation_size(size_t elementSize, size_t elementCount)
{
	size_t bufferSize;
	size_t allocSize;

	// FIXME: clang-tidy produces a false-positive warning for this
	// sequence because it does not understand that the overflow
	// builtins always write to their output parameters. The NOLINT
	// should be removed once this is fixed in clang-tidy.
	// See https://github.com/llvm/llvm-project/issues/136812

	// NOLINTBEGIN(clang-analyzer-core.CallAndMessage)
	bool overflow =
	  __builtin_mul_overflow(elementCount, elementSize, &bufferSize);
	static constexpr size_t CounterSize = sizeof(uint32_t);
	// We also need space for the header
	overflow |=
	  __builtin_add_overflow(sizeof(MessageQueue), bufferSize, &allocSize);
	// NOLINTEND(clang-analyzer-core.CallAndMessage)

	if (overflow)
	{
		return -EINVAL;
	}
	// We need the counters to be able to run to double the queue size without
	// hitting the high bits.  Error if this is the case.
	//
	// This should never be reached: a queue needs to be at least 256 MiB
	// (assuming one-byte elements) to hit this limit.
	if (((elementCount | (elementCount * 2)) &
	     HighBitFlagLock::reserved_bits()) != 0)
	{
		return -EINVAL;
	}

	return allocSize;
}

int queue_create(Timeout              *timeout,
                 AllocatorCapability   heapCapability,
                 struct MessageQueue **outQueue,
                 size_t                elementSize,
                 size_t                elementCount)
{
	ssize_t allocSize = queue_allocation_size(elementSize, elementCount);
	if (allocSize < 0)
	{
		return allocSize;
	}

	// Allocate the space for the queue.
	Capability buffer{heap_allocate(timeout, heapCapability, allocSize)};
	if (!buffer.is_valid())
	{
		return -ENOMEM;
	}

	*outQueue = new (buffer.get()) MessageQueue(elementSize, elementCount);
	return 0;
}

int queue_send_multiple(Timeout             *timeout,
                        struct MessageQueue *handle,
                        const void          *src,
                        size_t               count)
{
	Debug::log("Send called on: {}", handle);
	auto        *producer   = &handle->producer;
	auto        *consumer   = &handle->consumer;
	bool         shouldWake = false;
	volatile int ret        = 0;
	{
		Debug::log("Lock word: {}", producer->load());
		HighBitFlagLock l{*producer};
		if (LockGuard g{l, timeout})
		{
			// In an error-handling context, try to add the element to the
			// queue.  If the permissions on src are invalid, or either `handle`
			// or `src` is freed concurrently, we will hit the path that returns
			// -EPERM.  The counter update happens last, so any failure will
			// simply leave the queue in the old state.
			on_error(
			  [&] {
				  while (count > 0)
				  {
					  uint32_t producerCounter = counter_load(producer);
					  uint32_t consumerValue   = consumer->load();
					  uint32_t consumerCounter =
					    consumerValue & ~(HighBitFlagLock::reserved_bits());
					  Debug::log(
					    "Producer counter: {}, consumer counter: {}, Size: {}",
					    producerCounter,
					    consumerCounter,
					    handle->queueSize);
					  while (is_full(
					    handle->queueSize, producerCounter, consumerCounter))
					  {
						  // Wait on the value to change.  If we hit this path
						  // while the consumer lock is held, then the high bits
						  // will be set.  Make sure that we yield.
						  if (consumer->wait(timeout, consumerValue) ==
						      -ETIMEDOUT)
						  {
							  Debug::log("Timed out on futex (ret: {})", ret);
							  // If we haven't yet sent anything, report timeout
							  // failure, otherwise report the number that were
							  // sent.
							  if (ret == 0)
							  {
								  ret = -ETIMEDOUT;
							  }
							  return;
						  }
						  consumerValue = consumer->load();
						  consumerCounter =
						    consumerValue & ~(HighBitFlagLock::reserved_bits());
					  }
					  size_t startIndex =
					    index_at_counter(*handle, producerCounter);
					  size_t consumerIndex =
					    index_at_counter(*handle, consumerCounter);

					  // If the consumer is before the producer, the producer
					  // can use all of the space from the current space to the
					  // end.
					  if (consumerIndex <= startIndex)
					  {
						  consumerIndex = handle->queueSize;
					  }
					  size_t elementsToCopy =
					    std::min(count, consumerIndex - startIndex);
					  Debug::log(
					    "Send Copying {} elements (count: {}, consumer index: "
					    "{}, start index: {}",
					    elementsToCopy,
					    count,
					    consumerIndex,
					    startIndex);
					  memcpy(queue_pointer_at_index(*handle, startIndex),
					         static_cast<const char *>(src) +
					           (ret * handle->elementSize),
					         elementsToCopy * handle->elementSize);
					  ret += elementsToCopy;
					  count -= elementsToCopy;
					  counter_store(&handle->producer,
					                add_and_wrap(handle->queueSize,
					                             producerCounter,
					                             elementsToCopy));
					  // Check if the queue was empty before we updated the
					  // producer counter.  By the time that we reach this
					  // point, anything on the consumer side will be on the
					  // path to a futex_wait with the old version of the
					  // producer counter and so will bounce out again.
					  shouldWake |=
					    is_empty(producerCounter, counter_load(consumer));
				  }
			  },
			  [&]() {
				  ret = -EPERM;
				  Debug::log("Error in send");
			  });
		}
		else
		{
			Debug::log("Timed out on lock");
			return -ETIMEDOUT;
		}
	}
	if (shouldWake)
	{
		handle->producer.notify_all();
	}
	return ret;
}

int queue_send(Timeout *timeout, struct MessageQueue *handle, const void *src)
{
	return std::min(0, queue_send_multiple(timeout, handle, src, 1));
}

int queue_reset(Timeout *timeout, struct MessageQueue *queue)
{
	HighBitFlagLock producerLock{queue->producer};
	HighBitFlagLock consumerLock{queue->consumer};
	if (LockGuard producerGuard{producerLock, timeout})
	{
		if (LockGuard consumerGuard{consumerLock, timeout})
		{
			counter_store(&queue->producer, 0);
			counter_store(&queue->consumer, 0);
		}
	}
	queue->producer.notify_all();
	return -ETIMEDOUT;
}

int queue_receive_multiple(Timeout             *timeout,
                           struct MessageQueue *handle,
                           void                *dst,
                           size_t               count)
{
	Debug::log("Receive called on: {}", handle);
	auto        *producer   = &handle->producer;
	auto        *consumer   = &handle->consumer;
	bool         shouldWake = false;
	volatile int ret        = 0;
	{
		HighBitFlagLock l{*consumer};
		if (LockGuard g{l, timeout})
		{
			// In an error-handling context, try to add the element to the
			// queue.  If the permissions on `dst` are invalid, or either
			// `handle` or `dst` is freed concurrently, we will hit the path
			// that returns `-EPERM`.  The counter update happens last, so any
			// failure will simply leave the queue in the old state.
			on_error(
			  [&] {
				  while (count > 0)
				  {
					  uint32_t producerValue = producer->load();
					  uint32_t producerCounter =
					    producerValue & ~(HighBitFlagLock::reserved_bits());
					  uint32_t consumerCounter = counter_load(consumer);
					  Debug::log(
					    "Producer counter: {}, consumer counter: {}, Size: {}",
					    producerCounter,
					    consumerCounter,
					    handle->queueSize);
					  while (is_empty(producerCounter, consumerCounter))
					  {
						  // Wait on the value to change.  If we hit this path
						  // while the producer lock is held, then the high bits
						  // will be set.  Make sure that we yield.
						  if (producer->wait(timeout, producerValue) ==
						      -ETIMEDOUT)
						  {
							  Debug::log("Timed out on futex (ret: {})", ret);
							  // If we haven't yet  anything, report timeout
							  // failure, otherwise report the number that were
							  // sent.
							  if (ret == 0)
							  {
								  ret = -ETIMEDOUT;
							  }
							  return;
						  }
						  producerValue = producer->load();
						  producerCounter =
						    producerValue & ~(HighBitFlagLock::reserved_bits());
					  }
					  size_t startIndex =
					    index_at_counter(*handle, consumerCounter);
					  size_t producerIndex =
					    index_at_counter(*handle, producerCounter);

					  // If the producer is before the consumer, the producer
					  // has wrapped and we can read to the end of the buffer.
					  //
					  if (producerIndex <= startIndex)
					  {
						  producerIndex = handle->queueSize;
					  }
					  size_t elementsToCopy =
					    std::min(count, producerIndex - startIndex);
					  Debug::log(
					    "Copying {} elements (count: {}, producer index: "
					    "{}, start index: {}",
					    elementsToCopy,
					    count,
					    producerIndex,
					    startIndex);
					  Debug::log("Send copying {} bytes from {} to {}",
					             handle->elementSize,
					             dst,
					             queue_pointer_at_index(*handle, startIndex));
					  memcpy(static_cast<char *>(dst) +
					           (ret * handle->elementSize),
					         queue_pointer_at_index(*handle, startIndex),
					         elementsToCopy * handle->elementSize);
					  ret += elementsToCopy;
					  count -= elementsToCopy;
					  counter_store(&handle->consumer,
					                add_and_wrap(handle->queueSize,
					                             consumerCounter,
					                             elementsToCopy));
					  // Check if the queue was full before we updated the
					  // consumer counter.  By the time that we reach this
					  // point, anything on the producer side will be on the
					  // path to a futex_wait with the old version of the
					  // consumer counter and so will bounce out again.
					  shouldWake |= is_full(handle->queueSize,
					                        counter_load(producer),
					                        consumerCounter);
				  }
			  },
			  [&]() {
				  ret = -EPERM;
				  Debug::log("Error in receive");
			  });
		}
		else
		{
			Debug::log("Timed out on lock");
			return -ETIMEDOUT;
		}
	}
	// If the queue is concurrently freed, this can trap, but we won't leak any
	// locks.
	if (shouldWake)
	{
		handle->consumer.notify_all();
	}
	return ret;
}

int queue_receive(Timeout *timeout, struct MessageQueue *handle, void *dst)
{
	return std::min(0, queue_receive_multiple(timeout, handle, dst, 1));
}

int queue_items_remaining(struct MessageQueue *handle, size_t *items)
{
	auto producerCounter = counter_load(&handle->producer);
	auto consumerCounter = counter_load(&handle->consumer);
	*items =
	  items_remaining(handle->queueSize, producerCounter, consumerCounter);
	Debug::log("Producer counter: {}, consumer counter: {}, items: {}",
	           producerCounter,
	           consumerCounter,
	           *items);
	return 0;
}

void multiwaiter_queue_send_init(struct EventWaiterSource *source,
                                 struct MessageQueue      *handle)
{
	uint32_t producer   = counter_load(&handle->producer);
	uint32_t consumer   = counter_load(&handle->consumer);
	source->eventSource = &handle->consumer;
	source->value =
	  is_full(handle->queueSize, producer, consumer) ? consumer : -1;
}

void multiwaiter_queue_receive_init(struct EventWaiterSource *source,
                                    struct MessageQueue      *handle)
{
	uint32_t producer   = counter_load(&handle->producer);
	uint32_t consumer   = counter_load(&handle->consumer);
	source->eventSource = &handle->producer;
	source->value       = is_empty(producer, consumer) ? producer : -1;
}

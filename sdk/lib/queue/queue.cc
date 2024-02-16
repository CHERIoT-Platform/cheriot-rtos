#include <cheri.hh>
#include <cstdlib>
#include <errno.h>
#include <locks.hh>
#include <queue.h>
#include <stddef.h>
#include <stdint.h>
#include <timeout.h>
#include <type_traits>

using namespace CHERI;
using cheriot::atomic;

using Debug = ConditionalDebug<false, "Queue library">;

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
	 * Returns a pointer to the element in the queue indicated by `counter`.
	 */
	Capability<void> buffer_at_counter(struct QueueHandle &handle,
	                                   uint32_t            counter)
	{
		// Handle wrap for the second run around the counter.
		size_t index =
		  counter >= handle.queueSize ? counter - handle.queueSize : counter;
		auto             offset = index * handle.elementSize;
		Capability<void> pointer{handle.buffer};
		pointer.address() += offset;
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
		static constexpr uint32_t LockBit    = 1U << 31;
		static constexpr uint32_t WaitersBit = 1U << 30;

		// Function required to conform to the Lock concept.
		void lock()
		{
			__builtin_unreachable();
		}

		static constexpr uint32_t reserved_bits()
		{
			return LockBit | WaitersBit;
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

	/// Permissions for read-only access to a counter.
	static constexpr PermissionSet ReadOnly{Permission::Global,
	                                        Permission::Load};
	/// Permissions for read-only access to a buffer.
	static constexpr PermissionSet ReadOnlyCapability{
	  Permission::Global,
	  Permission::Load,
	  Permission::LoadStoreCapability,
	  Permission::LoadGlobal,
	  Permission::LoadGlobal};
	/// Permissions for write-only access to a buffer.
	static constexpr PermissionSet WriteOnlyCapability{
	  Permission::Global,
	  Permission::Store,
	  Permission::LoadStoreCapability};

	/**
	 * Helper to drop our short-lived claims.
	 */
	void drop_claims()
	{
		Timeout t{0};
		heap_claim_fast(&t, nullptr, nullptr);
	}

} // namespace

struct QueueHandle queue_make_receive_handle(struct QueueHandle handle)
{
	Capability buffer   = handle.buffer;
	Capability producer = handle.producer;
	buffer.permissions() &= ReadOnlyCapability;
	producer.permissions() &= ReadOnly;
	handle.buffer   = buffer;
	handle.producer = producer;
	return handle;
}

struct QueueHandle queue_make_send_handle(struct QueueHandle handle)
{
	Capability buffer   = handle.buffer;
	Capability consumer = handle.consumer;
	buffer.permissions() &= WriteOnlyCapability;
	consumer.permissions() &= ReadOnly;
	handle.buffer   = buffer;
	handle.consumer = consumer;
	return handle;
}

int queue_create(Timeout            *timeout,
                 struct SObjStruct  *heapCapability,
                 struct QueueHandle *outQueue,
                 void              **outAllocation,
                 size_t              elementSize,
                 size_t              elementCount)
{
	size_t bufferSize;
	size_t allocSize;
	bool   overflow =
	  __builtin_mul_overflow(elementCount, elementSize, &bufferSize);
	// We must be able to accurately represent the buffer, so round it up to a
	// representable length.
	bufferSize = CHERI::representable_length(bufferSize);
	static constexpr size_t CounterSize = sizeof(uint32_t);
	// Round up the size to be correctly aligned for the counters at the end
	// (if necessary) and add two counters worth of space.
	overflow |= __builtin_add_overflow(
	  bufferSize,
	  (2 * CounterSize) + (CounterSize - (bufferSize & (CounterSize - 1))),
	  &allocSize);
	if (overflow)
	{
		return -EINVAL;
	}
	// We need the counters to be able to run to double the queue size without
	// hitting the high bits.  Error if this is the case.
	//
	// This should never be reached: a queue needs to be at least 512 MiB
	// (assuming one-byte elements) to hit this limit.
	if (((elementCount | (elementCount * 2)) &
	     HighBitFlagLock::reserved_bits()) != 0)
	{
		return -EINVAL;
	}

	// Allocate the space for the queue.
	Capability buffer{heap_allocate(timeout, heapCapability, allocSize)};
	if (!buffer.is_valid())
	{
		return -ENOMEM;
	}

	Capability<std::atomic<uint32_t>> producer{
	  buffer.cast<std::atomic<uint32_t>>()};
	Capability<std::atomic<uint32_t>> consumer{
	  buffer.cast<std::atomic<uint32_t>>()};
	// Make the producer and consumer point after the buffer
	producer.address() += bufferSize;
	consumer.address() += bufferSize + CounterSize;
	// Set their bounds to 4 bytes.
	producer.bounds() = CounterSize;
	consumer.bounds() = CounterSize;
	// The pointer used to free the allocation
	*outAllocation  = buffer;
	buffer.bounds() = bufferSize;
	Debug::log("Created queue with buffer: {}", buffer);
	// The handle
	*outQueue = {elementSize, elementCount, buffer, producer, consumer};

	return 0;
}

int queue_send(Timeout *timeout, struct QueueHandle *handle, const void *src)
{
	Debug::log("Send called on: {}", handle);
	auto *producer   = handle->producer;
	auto *consumer   = handle->consumer;
	bool  shouldWake = false;
	{
		Debug::log("Lock word: {}", producer->load());
		HighBitFlagLock l{*producer};
		if (LockGuard g{l, timeout})
		{
			uint32_t producerCounter = counter_load(producer);
			uint32_t consumerCounter = counter_load(consumer);
			Debug::log("Producer counter: {}, consumer counter: {}, Size: {}",
			           producerCounter,
			           consumerCounter,
			           handle->queueSize);
			while (is_full(handle->queueSize, producerCounter, consumerCounter))
			{
				if (consumer->wait(timeout, consumerCounter) == -ETIMEDOUT)
				{
					Debug::log("Timed out on futex");
					return -ETIMEDOUT;
				}
				consumerCounter = counter_load(consumer);
			}
			auto entry = buffer_at_counter(*handle, producerCounter);
			if (int claim = heap_claim_fast(timeout, handle->buffer, src);
			    claim != 0)
			{
				Debug::log("Claim failed: {}", claim);
				return claim;
			}
			if (!check_pointer<PermissionSet{Permission::Load}, false>(
			      src, handle->elementSize))
			{
				drop_claims();
				Debug::log("Load / bounds check failed: {}");
				return -EPERM;
			}
			Debug::log("Send copying {} bytes from {} to {}",
			           handle->elementSize,
			           src,
			           entry);
			memcpy(entry, src, handle->elementSize);
			drop_claims();
			counter_store(
			  handle->producer,
			  increment_and_wrap(handle->queueSize, producerCounter));
			// Check if the queue was empty before we updated the producer
			// counter.  By the time that we reach this point, anything on the
			// consumer side will be on the path to a futex_wait with the old
			// version of the producer counter and so will bounce out again.
			shouldWake = is_empty(producerCounter, counter_load(consumer));
		}
		else
		{
			Debug::log("Timed out on lock");
			return -ETIMEDOUT;
		}
	}
	if (shouldWake)
	{
		handle->producer->notify_all();
	}
	return 0;
}

int queue_receive(Timeout *timeout, struct QueueHandle *handle, void *dst)
{
	Debug::log("Receive called on: {}", handle);
	auto *producer   = handle->producer;
	auto *consumer   = handle->consumer;
	bool  shouldWake = false;
	{
		HighBitFlagLock l{*consumer};
		if (LockGuard g{l, timeout})
		{
			uint32_t producerCounter = counter_load(producer);
			uint32_t consumerCounter = counter_load(consumer);
			Debug::log("Producer counter: {}, consumer counter: {}, Size: {}",
			           producerCounter,
			           consumerCounter,
			           handle->queueSize);
			while (is_empty(producerCounter, consumerCounter))
			{
				if (producer->wait(timeout, producerCounter) == -ETIMEDOUT)
				{
					return -ETIMEDOUT;
				}
				producerCounter = counter_load(producer);
			}
			auto entry = buffer_at_counter(*handle, consumerCounter);
			if (int claim = heap_claim_fast(timeout, handle->buffer, dst);
			    claim != 0)
			{
				return claim;
			}
			if (!check_pointer<PermissionSet{Permission::Store}, false>(
			      dst, handle->elementSize))
			{
				drop_claims();
				Debug::log("Check pointer failed with {} for {} byte write",
				           dst,
				           handle->elementSize);
				return -EPERM;
			}
			Debug::log("Receive copying {} bytes from {} to {}",
			           handle->elementSize,
			           entry,
			           dst);
			memcpy(dst, entry, handle->elementSize);
			drop_claims();
			counter_store(
			  consumer, increment_and_wrap(handle->queueSize, consumerCounter));
			// Check if the queue was full before we updated the consumer
			// counter.  By the time that we reach this point, anything on the
			// producer side will be on the path to a futex_wait with the old
			// version of the consumer counter and so will bounce out again.
			shouldWake = is_full(
			  handle->queueSize, counter_load(producer), consumerCounter);
		}
		else
		{
			Debug::log("Timed out on lock");
			return -ETIMEDOUT;
		}
	}
	if (shouldWake)
	{
		handle->consumer->notify_all();
	}
	return 0;
}

int queue_items_remaining(struct QueueHandle *handle, size_t *items)
{
	auto producerCounter = counter_load(handle->producer);
	auto consumerCounter = counter_load(handle->consumer);
	*items =
	  items_remaining(handle->queueSize, producerCounter, consumerCounter);
	Debug::log("Producer counter: {}, consumer counter: {}, items: {}",
	           producerCounter,
	           consumerCounter,
	           *items);
	return 0;
}

void multiwaiter_queue_send_init(struct EventWaiterSource *source,
                                 struct QueueHandle       *handle)
{
	uint32_t producer   = counter_load(handle->producer);
	uint32_t consumer   = counter_load(handle->consumer);
	source->eventSource = handle->consumer;
	source->value =
	  is_full(handle->queueSize, producer, consumer) ? consumer : -1;
}

void multiwaiter_queue_receive_init(struct EventWaiterSource *source,
                                    struct QueueHandle       *handle)
{
	uint32_t producer   = counter_load(handle->producer);
	uint32_t consumer   = counter_load(handle->consumer);
	source->eventSource = handle->producer;
	source->value       = is_empty(producer, consumer) ? producer : -1;
}


#pragma once
#include <limits>
#include <locks.hh>
#include <thread.h>
#include <utils.hh>

/**
 * A simple single-producer, single-consumer ring buffer that can be made
 * multi-producer and / or multi-consumer by adding locking.  The first two
 * template parameters are the type of the message and the size of the buffer
 * (in messages).  The next template arguments are the locks to use for
 * protecting the producer and consumer ends.
 *
 * In the uncontended (or single-producer, single-consumer) case where the
 * producer and consumer are balanced and the ring is neither full nor empty,
 * this will not use any cross-compartment calls.  When the queue is full,
 * producers will block on a futex.  When the queue is empty, consumers will
 * block on a futex.  On transitions to non-empty and non-full, the queue will
 * issue a `futex_wake` cross-compartment call. If locks are used, they may
 * introduce additional cross-compartment calls.
 *
 * Note: The size must be a power of two.
 */
template<typename Message,
         size_t BufferSize,
         typename PushLock = NoLock,
         typename PopLock  = NoLock>
class RingBuffer
{
	/// The ring buffer.
	std::array<Message, BufferSize> ring;
	/// The lock protecting the push end, if one exists.
	[[no_unique_address]] PushLock pushLock;
	/// The lock protecting the pop end, if one exists.
	[[no_unique_address]] PopLock popLock;
	/// Free-running producer counter.
	_Atomic(uint32_t) producer;
	/// Free-running consumer counter.
	_Atomic(uint32_t) consumer;

	static_assert(BufferSize < std::numeric_limits<uint32_t>::max() / 2,
	              "The buffer size cannot be more than half the range of the "
	              "counter types or overflow will give incorrect values");
	static_assert((1 << utils::log2<BufferSize>()) == BufferSize,
	              "Buffer size must be a power of two");

	/**
	 * Helper to check if the ring is full.  The counters are free running and
	 * so the displacement between them (wrapping for overflow) will be the
	 * number of elements in the buffer if the ring is full.
	 */
	bool is_full()
	{
		return producer - consumer == BufferSize;
	}

	/**
	 * Check if the queue is empty.  If the consumer counter has caught up with
	 * the producer then this ring is empty.
	 */
	bool is_empty()
	{
		return producer == consumer;
	}

	/**
	 * Helper to convert the counter value to an array index.  The counters are
	 * never reset and so this is simple modulo arithmetic.  Power of two sizes
	 * are more efficient because they become bit masks.
	 */
	size_t counter_to_index(size_t counter)
	{
		return counter % BufferSize;
	}

	public:
	/**
	 * Push an element into the ring.  The caller is responsible for ensuring
	 * that, if this contains pointers, they have the global permission and so
	 * the move operation will not fault.
	 */
	void push(Message &&message)
	{
		LockGuard g{pushLock};
		// Wait for the queue to not be full
		while (is_full())
		{
			futex_wait(reinterpret_cast<uint32_t *>(&consumer),
			           producer - BufferSize);
		}
		auto i        = counter_to_index(producer);
		ring[i]       = std::move(message);
		bool wasEmpty = is_empty();
		producer++;
		g.unlock();
		if (wasEmpty)
		{
			futex_wake(reinterpret_cast<uint32_t *>(&producer),
			           std::numeric_limits<uint32_t>::max());
		}
	}

	/**
	 * Pop the next message from the queue.
	 */
	Message pop()
	{
		Message   result;
		LockGuard g{popLock};
		// Wait for the queue to not be full
		while (is_empty())
		{
			futex_wait(reinterpret_cast<uint32_t *>(&producer), consumer);
		}
		auto i       = counter_to_index(consumer);
		result       = std::move(ring[i]);
		bool wasFull = is_full();
		consumer++;
		g.unlock();
		if (wasFull)
		{
			futex_wake(reinterpret_cast<uint32_t *>(&consumer),
			           std::numeric_limits<uint32_t>::max());
		}
		return result;
	}
};

// Make sure that locks consume no space if not used.
static_assert(sizeof(RingBuffer<uint32_t, 2>) == 16);

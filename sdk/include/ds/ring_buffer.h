// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * @file Ring/circular buffer state machine.
 */

#pragma once

#include <cdefs.h>
#include <type_traits>

namespace ds::ring_buffer
{
	/**
	 * A statically-sized, non-atomic ring buffer state machine.  The actual
	 * element storage must be provided externally; all this does is manage the
	 * cursors.  This representation uses an explicit empty flag, rather than
	 * considering equal cursors to be empty, so as to not need an extra element
	 * of store Capacity-many items.
	 *
	 * The type of the cursors is parametric, but must be unsigned.
	 *
	 * The consumer workflow is:
	 *   - use head_get() to test emptiness and, if nonempty, read the index
	 *     of the head element
	 *   - make use of the element in the (external!) storage at that index
	 *   - call head_advance() to discard the head element
	 *
	 * The producer workflow is:
	 *   - use tail_next() to retrieve the index for the next element,
	 *     if there is room
	 *   - populate the element in the (external!) storage at that index
	 *   - call tail_advance() to make that element available to the
	 *     consumer
	 */
	template<typename Debug, std::size_t Capacity, typename _Ix = std::size_t>
	class Cursors
	{
		static_assert(std::is_arithmetic_v<_Ix> && std::is_unsigned_v<_Ix>,
		              "Ring buffer cursor type must be unsigned arithmetic");

		template<typename... Args>
		using Assert = typename Debug::template Assert<Args...>;

		public:
		using Ix = _Ix;

		private:
		Ix   head;
		Ix   tail;
		bool empty;

		Ix advance(Ix v)
		{
			if constexpr ((Capacity & (Capacity - 1)) == 0)
			{
				/* For power of two sizes, we can rely on bit masking. */
				return (v - 1) & (Capacity - 1);
			}
			else
			{
				/*
				 * For non-power-of-two, we can use unsigned underflow and
				 * signed arithmetic right shift to avoid branching.  mask is
				 * all zeros if next is non-negative and all ones otherwise.
				 */
				Ix next   = v - 1;
				using SIx = std::make_signed_t<Ix>;
				Ix mask   = static_cast<SIx>(next) >> (8 * sizeof(SIx) - 1);
				return (mask & (Capacity - 1)) | (~mask & next);
			}
		}

		public:
		/**
		 * Reset cursors to an empty state.
		 */
		void reset()
		{
			tail  = 0;
			head  = advance(tail);
			empty = true;
		}

		/**
		 * Is this ring empty?
		 */
		__always_inline bool is_empty()
		{
			return empty;
		}

		/**
		 * Try to retrieve the head index; returns true if available and
		 * false otherwise.
		 */
		bool head_get(Ix &v)
		{
			if (is_empty())
			{
				return false;
			}

			v = head;
			return true;
		}

		/**
		 * Discard the element at the index returned by head_get().
		 *
		 * Do not call this unless there was a matching call to
		 * head_get() that has returned true.
		 */
		__always_inline void head_advance()
		{
			Assert<>(!empty, "Cannot advance head of empty ring buffer!");
			if (head == tail)
			{
				empty = true;
			}
			head = advance(head);
		}

		/**
		 * Try to retrieve the index of the tail, if nonempty.
		 */
		bool tail_get(Ix &v)
		{
			if (is_empty())
			{
				return false;
			}

			v = tail;
			return true;
		}

		/**
		 * Try to retrieve the index beyond the tail, if there is room.
		 * Returns true and sets the index if so, or returns false if
		 * not.
		 */

		bool tail_next(Ix &v)
		{
			Ix nt = advance(tail);
			if (!empty && head == nt)
			{
				return false;
			}

			v = nt;
			return true;
		}

		/**
		 * Make the next element available.
		 *
		 * Do not call this unless there was a matching call to
		 * tail_next() that returned true.
		 */
		__always_inline void tail_advance()
		{
			Assert<>(empty || head != advance(tail),
			         "Cannot advance tail of full ring buffer!");
			tail  = advance(tail);
			empty = false;
		}
	};
} // namespace ds::ring_buffer

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <debug.hh>
#include <stdint.h>

/**
 * Object representing a compressed pointer to a heap resident object of type T.
 *
 * A HeapOffset with `value = 0` represents a `nullptr`.
 */
template<typename T>
struct HeapOffset
{
	/// The compressed pointer value as an offset into the allocator heap.
	uint16_t value = 0;

	/**
	 * Return true if the underlying value is 0, that is, if this offset
	 * represents a `nullptr`.
	 */
	[[nodiscard]] bool is_null() const
	{
		return value == 0;
	}

	/**
	 * Decode this offset and return a pointer to T, or `nullptr` if null.
	 */
	[[nodiscard]] T *get() const;

	/**
	 * Encode a pointer to a heap-allocated T as a HeapOffset.
	 * Asserts that `ptr` is correctly aligned and is within the
	 * representable range of 16 bits.
	 */
	[[nodiscard]] static HeapOffset<T> from(T *ptr);

	/**
	 * Comparison operators compare the underlying compressed pointer value.
	 */
	bool operator==(const HeapOffset &other) const
	{
		return value == other.value;
	}

	bool operator!=(const HeapOffset &other) const
	{
		return value != other.value;
	}

	/**
	 * Dereference operators allow this to be used as any other pointer.
	 */
	T *operator->() const
	{
		return get();
	}

	T &operator*() const
	{
		return *get();
	}
};

template<typename T>
struct DebugFormatArgumentAdaptor<HeapOffset<T>>
{
	__always_inline static DebugFormatArgument construct(const HeapOffset<T> &h)
	{
		return DebugFormatArgumentAdaptor<uint16_t>::construct(h.value);
	}
};

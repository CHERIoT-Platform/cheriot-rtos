// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * @file Pointer utilities
 */

#pragma once

#include <cheri.hh>

namespace ds::pointer
{

	/**
	 * Offset a pointer by a number of bytes.  The return type must be
	 * explicitly specified by the caller.  The type of the displacement offset
	 * (Offset) is templated so that we can accept both signed and unsigned
	 * offsets.
	 */
	template<typename T, typename U>
	static inline __always_inline T *offset(U *base, std::integral auto offset)
	{
		CHERI::Capability c{base};
		c.address() += offset;
		return c.template cast<void>().template cast<T>();
	}

	/**
	 * Compute the unsigned difference in bytes between two pointers' target
	 * addresses.  To be standards-compliant, cursor must be part of the same
	 * allocation as base and at a higher address.
	 */
	static inline __always_inline size_t diff(const void *base,
	                                          const void *cursor)
	{
		return static_cast<size_t>(reinterpret_cast<const char *>(cursor) -
		                           reinterpret_cast<const char *>(base));
	}

} // namespace ds::pointer

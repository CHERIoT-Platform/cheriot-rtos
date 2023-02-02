// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * @file Bit manipulation utilities
 */

#pragma once

#include <concepts>

namespace ds::bits
{

	/**
	 * Isolate the least significant set bit.  That is, clear all bits to the
	 * left of the first set bit.
	 */
	template<typename T>
	__always_inline T isolate_least(T v)
	{
		return v & -v;
	}

	/**
	 * Mask of all bits above and including the least significant set bit.
	 */
	template<typename T>
	__always_inline T above_or_least(T v)
	{
		return v | -v;
	}

	/**
	 * Mask of all bits above the least significant set bit.
	 */
	template<typename T>
	__always_inline T above_least(T v)
	{
		return above_or_least(v << 1);
	}

} // namespace ds::bits

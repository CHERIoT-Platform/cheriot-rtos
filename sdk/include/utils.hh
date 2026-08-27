// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * \file
 * \brief Miscellaneous utility functions and classes.
 */

#pragma once

#include <cdefs.h>
#include <limits>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>
#include <utility>

namespace utils
{
	/// Turn a byte count into a bit count
	constexpr size_t bytes2bits(size_t in)
	{
		return in * __CHAR_BIT__;
	}

	/// Compute the log base 2 of a power of 2
	template<size_t N>
	constexpr size_t log2()
	{
		static_assert(N > 0 && (N & (N - 1)) == 0);

		return 1U + log2<(N >> 1)>();
	}
	template<>
	constexpr size_t log2<1U>()
	{
		return 0;
	}

	/// Return the extent of an array
	template<typename T, size_t N>
	[[deprecated("Use std::extent_v<decltype(arr)> for array_size(arr)")]]
	constexpr size_t array_size(T (&a)[N])
	{
		return N;
	}

	/**
	 * Divide `value` by `divisor`, rounding up, unlike `/`.
	 *
	 * If `value + divisor` overflows their type `T`, the result is
	 * correct in a modular sense, but likely not useful in practice.
	 */
	template<typename T>
	    requires std::is_integral_v<T>
	constexpr T round_up_divide(T value, T divisor)
	{
		return (value + divisor - 1) / divisor;
	}

	/**
	 * Return the smallest `multiple` greater than or equal to `value`.
	 *
	 * If `value + multiple` overflows their type `T`, the result is
	 * correct in a modular sense, but likely not useful in practice.
	 */
	template<typename T>
	    requires std::is_integral_v<T>
	constexpr T align_up(T value, T multiple)
	{
		return round_up_divide(value, multiple) * multiple;
	}
	static_assert(align_up(15, 16) == 16);
	static_assert(align_up(28, 16) == 32);
	static_assert(align_up(17, 8) == 24);

	/**
	 * Return the smallest `Multiple` greater than or equal to `value`,
	 * for the special case where Multiple is a static power of two.
	 *
	 * If `value + Multiple` overflows the type `T`, the result is correct
	 * in a modular sense, but likely not useful in practice.
	 */
	template<auto Multiple, typename T>
	    requires std::is_integral_v<T> && (std::in_range<T>(Multiple)) &&
		         ((Multiple & (Multiple - 1)) == 0)
	constexpr T align_up(T value)
	{
		return (value + Multiple - 1) & -Multiple;
	}
	static_assert(align_up<16>(15) == 16);
	static_assert(align_up<16>(28) == 32);
	static_assert(align_up<8>(17) == 24);

	/**
	 * \brief Utility class to delete copy and move contructors.
	 *
	 * Inherit from this if want to prevent your class from being accidentally
	 * copied. This is especially useful for classes used for MMIO as they
	 * must be instantiated at a specific address so copying is a bad idea.
	 *
	 * The default no-argument constructor and destructor is provided.
	 */
	class NoCopyNoMove
	{
		public:
		NoCopyNoMove()                                = default;
		NoCopyNoMove(const NoCopyNoMove &)            = delete;
		NoCopyNoMove &operator=(const NoCopyNoMove &) = delete;
		NoCopyNoMove(NoCopyNoMove &&)                 = delete;
		NoCopyNoMove &operator=(NoCopyNoMove &&)      = delete;
		~NoCopyNoMove()                               = default;
	};

	/**
	 * A helper class modelled on `std::optional` that represents an optional
	 * `T&`.  This is stored as a pointer with `nullptr` representing the
	 * not-present version.
	 *
	 * Unlike `std::optional`, this intentionally omits the APIs that make it
	 * possible to access the value without checking that it is present.
	 *
	 * This is intended to be used as an alternative to using bare pointers to
	 * represent `T& | None`.
	 */
	template<typename T>
	class OptionalReference
	{
		/// The pointer to the real value
		T *pointer;

		public:
		/**
		 * Construct the optional wrapper from a real value.
		 */
		__always_inline OptionalReference(T &value) : pointer(&value) {}

		/**
		 * Construct the optional wrapper from not-present value.
		 */
		OptionalReference(std::nullptr_t) : pointer(nullptr) {}

		/**
		 * Returns a copy of the wrapped value if present or the provided
		 * default value if not.
		 */
		T value_or(T defaultValue)
		{
			if (pointer == nullptr)
			{
				return defaultValue;
			}
			return *pointer;
		}

		/**
		 * Returns a reference to the wrapped value if present or the provided
		 * default value if not.
		 */
		T &value_or(T &defaultValue)
		{
			if (pointer == nullptr)
			{
				return defaultValue;
			}
			return *pointer;
		}

		/**
		 * If this object holds a value then apply `f` to it and return the
		 * result, otherwise return the result of converting nullptr to the
		 * return type of `f`.
		 */
		__always_inline auto and_then(auto &&f)
		{
			using Result = decltype(f(std::declval<T &>()));
			if constexpr (std::is_same_v<void, Result>)
			{
				if (pointer != nullptr)
				{
					f(*pointer);
				}
				return;
			}
			else
			{
				if (pointer != nullptr)
				{
					return f(*pointer);
				}
				return Result{nullptr};
			}
		}
	};

} // namespace utils

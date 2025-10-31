// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <limits>
#include <stddef.h>
#include <stdint.h>
#include <type_traits>

namespace utils
{
	constexpr size_t bytes2bits(size_t in)
	{
		return in * __CHAR_BIT__;
	}

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

	template<typename T, size_t N>
	constexpr size_t array_size(T (&a)[N])
	{
		return N;
	}

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

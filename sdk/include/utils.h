// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <stddef.h>

#ifdef __cplusplus
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
		NoCopyNoMove()                     = default;
		NoCopyNoMove(const NoCopyNoMove &) = delete;
		NoCopyNoMove &operator=(const NoCopyNoMove &) = delete;
		NoCopyNoMove(NoCopyNoMove &&)                 = delete;
		NoCopyNoMove &operator=(NoCopyNoMove &&) = delete;
		~NoCopyNoMove()                          = default;
	};

} // namespace utils
#endif // __cplusplus

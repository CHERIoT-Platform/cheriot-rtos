// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <functional>
#include <timeout.h>

/**
 * Helper to turn a function that takes a timeout into one that may block
 * forever.
 */
template<auto Fn, typename... Args>
__always_inline auto blocking_forever(Args... args)
{
	return Fn(TimeoutWaitForever, std::forward<Args>(args)...);
}

/**
 * Helper to turn a function that takes a timeout into one that may not yield
 */
template<auto Fn, typename... Args>
__always_inline auto non_blocking(Args... args)
{
	return Fn(TimeoutNoWait, std::forward<Args>(args)...);
}

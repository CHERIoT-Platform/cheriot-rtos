// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <atomic>
#include <cancellation.h>

/**
 * Cancellation token state.
 */
struct CancellationTokenState
{
	/**
	 * The flag indicating that the token has been signalled.
	 */
	std::atomic<bool> token;
};

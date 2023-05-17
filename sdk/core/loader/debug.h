// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <array>
#include <cheri.hh>
#include <debug.hh>

namespace
{
	/**
	 * Is the loader being debugged?
	 */
	static constexpr bool DebugLoader = DEBUG_LOADER;

	/**
	 * Debug interface for the loader.  Enables verbose debugging if
	 * `DebugLoader` is true and requires that the loader explicitly
	 * initialises the UART.
	 */
	using Debug = ConditionalDebug<DebugLoader,
	                               "Loader",
	                               MessageBuilder<ExplicitUARTOutput>>;
} // namespace

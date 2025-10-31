// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <concepts>
#include <stdint.h>

/**
 * Concept for checking that a UART driver exposes the right interface.
 */
template<typename T>
concept IsUart = requires(volatile T *v, uint8_t byte) {
	{ v->init() };
	{ v->can_write() } -> std::same_as<bool>;
	{ v->can_read() } -> std::same_as<bool>;
	{ v->blocking_read() } -> std::same_as<uint8_t>;
	{ v->blocking_write(byte) };
};

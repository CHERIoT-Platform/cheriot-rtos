// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <concepts>

namespace ds
{

	/**
	 * Concepts meant to label functions as field projectors and their
	 * container_of-like inverses.
	 *
	 * @{
	 */

	template<auto F, typename Object, typename Field>
	concept IsFieldOf = requires(Object *o) {
		{ F(o) } -> std::same_as<Field *>;
	};

	template<auto F, typename Object, typename Field>
	concept IsContainerOf = requires(Field *c) {
		{ F(c) } -> std::same_as<Object *>;
	};

	/** @} */

} // namespace ds

// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <__macro_map.h>
#include <concepts>
#include <type_traits>

/**
 * Compute the bitwise OR of zero or more enumeration values.
 *
 * E is inferrable if one or more enumeration values are given.
 */
template<typename E, typename... Es>
    requires std::is_enum_v<E> &&
             ((sizeof...(Es) == 0) ||
              (std::same_as<std::remove_cv_t<E>,
                            std::remove_cv_t<std::common_type_t<Es...>>>))
constexpr E enum_or(E v0, Es... vs)
{
	using T = std::underlying_type_t<E>;
	/* NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange) */
	T res = static_cast<T>(v0);
	for (E v : std::initializer_list<E>{vs...})
	{
		res |= static_cast<T>(v);
	}
	return static_cast<E>(res);
	/* NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange) */
}

/// The binary OR of no values is 0.
template<typename E>
    requires std::is_enum_v<E>
constexpr E enum_or()
{
	return static_cast<E>(0);
}

/**
 * Compute the bitwise OR of zero or more enumeration values whose type is given
 * as the first argument (which may be a type-level expression evaluating to the
 * desired enumeration type).
 */
#define ENUM_OR(EE, ...)                                                       \
	({                                                                         \
		using E = EE;                                                          \
		static_assert(std::is_enum_v<E>);                                      \
		using enum E;                                                          \
		enum_or<E>(__VA_ARGS__);                                               \
	})

/**
 * Assign v to the bitwise OR of zero or more enumeration values of its type.
 */
#define ENUM_ASSIGN_OR(a, ...)                                                 \
	{                                                                          \
		a = ENUM_OR(decltype(a), __VA_ARGS__);                                 \
	}

/**
 * ENUM_CLEAR_THEN_SET(e, (cs...), (ss...)) computes the value of e with cs...
 * cleared and ss... set.
 *
 * Specifically, it is the value of e modified such that
 *   - any set bit in any cs... value is cleared, and then
 *   - any set bit in any ss... value is set, and
 *   - all bits set in no cs... value and in no ss... value are preserved.
 *
 * The parentheses around (cs...) and (ss...) are required and either or both
 * may be empty.
 */
#define ENUM_CLEAR_THEN_SET(e, cs, ss)                                         \
	({                                                                         \
		using E = decltype(e);                                                 \
		static_assert(std::is_enum_v<E>);                                      \
		using T    = std::underlying_type_t<E>;                                \
		const T ev = static_cast<T>(e);                                        \
		const T cv = static_cast<T>(ENUM_OR(E, CHERIOT_EVAL0 cs));             \
		const T sv = static_cast<T>(ENUM_OR(E, CHERIOT_EVAL0 ss));             \
		/* NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange) */        \
		static_cast<E>((ev & ~cv) | sv);                                       \
		/* NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange) */          \
	})

/**
 * Modify a mutable location with the effect of ENUM_CLEAR_THEN_SET.
 */
#define ENUM_UPDATE_CLEAR_THEN_SET(e, cs, ss)                                  \
	{                                                                          \
		(e) = ENUM_CLEAR_THEN_SET(e, cs, ss);                                  \
	}

/**
 * ENUM_MASKED_EQUAL(e, m, v) is true iff (e & m) == v
 */
#define ENUM_MASKED_EQUAL(e, m, v)                                             \
	({                                                                         \
		using E = decltype(e);                                                 \
		static_assert(std::is_enum_v<E>);                                      \
		using T = std::underlying_type_t<E>;                                   \
		using enum E;                                                          \
		/* NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange) */        \
		(static_cast<T>(e) & static_cast<T>(m)) == static_cast<T>(v);          \
		/* NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange) */          \
	})

/**
 * ENUM_ARE_ALL_SET(e, vs...) is true iff all asserted bits in the OR of all
 * vs... values are asserted in e.
 *
 * If e is a collection of flags, this is equivalent to asking if all flags
 * vs... are asserted.  Avoid the use of non-flags (multi-bit field masks or
 * values) in vs... .
 */
#define ENUM_ARE_ALL_SET(e, ...)                                               \
	({                                                                         \
		const auto m = ENUM_OR(decltype(e), __VA_ARGS__);                      \
		ENUM_MASKED_EQUAL(e, m, m);                                            \
	})

/**
 * ENUM_IS_SET(e, q) asks if the asserted bit(s) in q are set in e.  Best avoid
 * non-flag (multi-bit field masks or values) q.  For conjuctive queries,
 * consider ENUM_ARE_ALL_SET instead.
 */
#define ENUM_IS_SET(e, q) ENUM_ARE_ALL_SET(e, q)

/**
 * ENUM_IS_ANY_SET(e, vs...) is true iff any of the asserted bits in the OR of
 * all vs... values is asserted in e.
 */
#define ENUM_IS_ANY_SET(v, ...)                                                \
	({                                                                         \
		using E = decltype(v);                                                 \
		!ENUM_MASKED_EQUAL(v, enum_or<E>(__VA_ARGS__), static_cast<E>(0));     \
	})

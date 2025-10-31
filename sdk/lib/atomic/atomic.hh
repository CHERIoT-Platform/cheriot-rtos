// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <stdint.h>

/**
 * The helper functions need to expose an unmangled name because the compiler
 * inserts calls to them.  Declare them using the asm label extension.
 */
#define DECLARE_ATOMIC_LIBCALL(name, ret, ...)                                 \
	[[cheriot::interrupt_state(disabled)]] CHERIOT_DECLARE_STANDARD_LIBCALL(   \
	  name, ret, __VA_ARGS__)

/**
 * Macro that defines a library function to implement an atomic load for the
 * specified size, using the specified type.
 *
 * Ideally the compiler should know that a system with a single core can do a
 * single load for most cases of this, rather than needing a cross-library
 * call, so eventually this will go away.
 */
#define DEFINE_ATOMIC_LOAD(size, type)                                         \
	DECLARE_ATOMIC_LIBCALL(__atomic_load_##size, type, const type *, int);     \
	type __atomic_load_##size(const type *ptr, int)                            \
	{                                                                          \
		static_assert(sizeof(type) == size, "Invalid type for size");          \
		return *ptr;                                                           \
	}

/**
 * Macro that defines a library function to implement an atomic store for the
 * specified size, using the specified type.
 *
 * Ideally the compiler should know that a system with a single core can do a
 * single store for most cases of this, rather than needing a cross-library
 * call, so eventually this will go away.
 */
#define DEFINE_ATOMIC_STORE(size, type)                                        \
	DECLARE_ATOMIC_LIBCALL(__atomic_store_##size, void, type *, type, int);    \
	void __atomic_store_##size(type *ptr, type value, int)                     \
	{                                                                          \
		static_assert(sizeof(type) == size, "Invalid type for size");          \
		*ptr = value;                                                          \
	}

/**
 * Macro that defines a library function to implement an atomic
 * fetch-and-something for the specified size, using the specified type.  The
 * name is the name of the operation and `op` is the C++ operator that
 * implements it.
 */
#define DEFINE_ATOMIC_FETCH_OP(size, type, name, op)                           \
	DECLARE_ATOMIC_LIBCALL(                                                    \
	  __atomic_fetch_##name##_##size, type, type *, type, int);                \
	type __atomic_fetch_##name##_##size(type *ptr, type value, int)            \
	{                                                                          \
		static_assert(sizeof(type) == size, "Invalid type for size");          \
		type          tmp = *ptr;                                              \
		*ptr              = tmp op value;                                      \
		return tmp;                                                            \
	}

/**
 * Helper macro that defines all of the cases of atomic operations.
 */
#define DEFINE_ATOMIC_FETCH_OPS(size, type)                                    \
	DEFINE_ATOMIC_FETCH_OP(size, type, add, +)                                 \
	DEFINE_ATOMIC_FETCH_OP(size, type, sub, -)                                 \
	DEFINE_ATOMIC_FETCH_OP(size, type, and, &)                                 \
	DEFINE_ATOMIC_FETCH_OP(size, type, or, |)                                  \
	DEFINE_ATOMIC_FETCH_OP(size, type, xor, ^)

/**
 * Macro that defines a library function to implement an atomic exchange for the
 * specified size, using the specified type.
 */
#define DEFINE_ATOMIC_EXCHANGE(size, type)                                     \
	DECLARE_ATOMIC_LIBCALL(__atomic_exchange_##size, type, type *, type, int); \
	type __atomic_exchange_##size(type *ptr, type value, int)                  \
	{                                                                          \
		static_assert(sizeof(type) == size, "Invalid type for size");          \
		type tmp = *ptr;                                                       \
		*ptr     = value;                                                      \
		return tmp;                                                            \
	}

/**
 * Macro that defines a library function to implement an atomic compare and
 * exchange for the specified size, using the specified type.
 */
#define DEFINE_ATOMIC_COMPARE_EXCHANGE(size, type)                             \
	DECLARE_ATOMIC_LIBCALL(                                                    \
	  __atomic_compare_exchange_##size, int, type *, type *, type, int, int);  \
	int __atomic_compare_exchange_##size(                                      \
	  type *ptr, type *expected, type desired, int, int)                       \
	{                                                                          \
		static_assert(sizeof(type) == size, "Invalid type for size");          \
		if (*ptr == *expected)                                                 \
		{                                                                      \
			*ptr = desired;                                                    \
			return 1;                                                          \
		}                                                                      \
		*expected = *ptr;                                                      \
		return 0;                                                              \
	}
#define DEFINE_ALL_ATOMIC_OPS(size, type)                                      \
	DEFINE_ATOMIC_LOAD(size, type)                                             \
	DEFINE_ATOMIC_STORE(size, type)                                            \
	DEFINE_ATOMIC_EXCHANGE(size, type)                                         \
	DEFINE_ATOMIC_COMPARE_EXCHANGE(size, type)                                 \
	DEFINE_ATOMIC_FETCH_OPS(size, type)

#pragma once
/**
 * This file implements the C11 and C++23 C atomics interfaces.  On targets
 * without hardware atomics, these will all lower to calls into the atomics
 * shared library.  You must link either atomics or atomics_fixed (if you use
 * only fixed-width atomics) into your firmware image.
 *
 * *WARNING*: The C++ atomics interface is more efficient for non-primitive
 * types but is *not* guaranteed to be interoperable with the C version.
 * Interoperable code should use only primitive types in atomics.
 */

#ifdef __cplusplus
#	include <atomic>
#	define _Atomic(T) std::atomic<T>
#else
#	include <stddef.h>
#	include <stdint.h>
enum memory_order
{
	memory_order_relaxed = __ATOMIC_RELAXED,
	memory_order_consume = __ATOMIC_CONSUME,
	memory_order_acquire = __ATOMIC_ACQUIRE,
	memory_order_release = __ATOMIC_RELEASE,
	memory_order_acq_rel = __ATOMIC_ACQ_REL,
	memory_order_seq_cst = __ATOMIC_SEQ_CST,
};

typedef _Atomic(_Bool) atomic_flag;

#	define ATOMIC_FLAG_INIT false

// Clang thinks that all atomics are too big, so ignore it.
__clang_ignored_warning_push("-Watomic-alignment")

__always_inline _Bool
atomic_flag_test_and_set_explicit(volatile atomic_flag *obj, enum memory_order order)
{
	return __c11_atomic_exchange(obj, 1, order);
}

__always_inline _Bool atomic_flag_test_and_set(volatile atomic_flag *obj)
{
	return atomic_flag_test_and_set_explicit(obj, memory_order_seq_cst);
}

__always_inline _Bool
atomic_flag_test_and_clear_explicit(volatile atomic_flag *obj,
                                    enum memory_order          order)
{
	return __c11_atomic_exchange(obj, 0, order);
}

__always_inline _Bool atomic_flag_test_and_clear(volatile atomic_flag *obj)
{
	return atomic_flag_test_and_clear_explicit(obj, memory_order_seq_cst);
}

__clang_ignored_warning_pop()


// The functions in the following block are mapped directly to builtins.
#	define atomic_init(obj, value) __c11_atomic_init(obj, value)
#	define atomic_compare_exchange_strong_explicit(                           \
	  object, expected, desired, success, failure)                             \
		__c11_atomic_compare_exchange_strong(                                  \
		  object, expected, desired, success, failure)
#	define atomic_compare_exchange_weak_explicit(                             \
	  object, expected, desired, success, failure)                             \
		__c11_atomic_compare_exchange_weak(                                    \
		  object, expected, desired, success, failure)
#	define atomic_exchange_explicit(object, desired, order)                   \
		__c11_atomic_exchange(object, desired, order)
#	define atomic_fetch_add_explicit(object, operand, order)                  \
		__c11_atomic_fetch_add(object, operand, order)
#	define atomic_fetch_and_explicit(object, operand, order)                  \
		__c11_atomic_fetch_and(object, operand, order)
#	define atomic_fetch_or_explicit(object, operand, order)                   \
		__c11_atomic_fetch_or(object, operand, order)
#	define atomic_fetch_sub_explicit(object, operand, order)                  \
		__c11_atomic_fetch_sub(object, operand, order)
#	define atomic_fetch_xor_explicit(object, operand, order)                  \
		__c11_atomic_fetch_xor(object, operand, order)
#	define atomic_load_explicit(object, order) __c11_atomic_load(object, order)
#	define atomic_store_explicit(object, desired, order)                      \
		__c11_atomic_store(object, desired, order)

// The functions in the following block are convenience wrappers around the
// previous block

#	define atomic_compare_exchange_strong(object, expected, desired)          \
		atomic_compare_exchange_strong_explicit(object,                        \
		                                        expected,                      \
		                                        desired,                       \
		                                        memory_order_seq_cst,          \
		                                        memory_order_seq_cst)
#	define atomic_compare_exchange_weak(object, expected, desired)            \
		atomic_compare_exchange_weak_explicit(object,                          \
		                                      expected,                        \
		                                      desired,                         \
		                                      memory_order_seq_cst,            \
		                                      memory_order_seq_cst)
#	define atomic_exchange(object, desired)                                   \
		atomic_exchange_explicit(object, desired, memory_order_seq_cst)
#	define atomic_fetch_add(object, operand)                                  \
		atomic_fetch_add_explicit(object, operand, memory_order_seq_cst)
#	define atomic_fetch_and(object, operand)                                  \
		atomic_fetch_and_explicit(object, operand, memory_order_seq_cst)
#	define atomic_fetch_or(object, operand)                                   \
		atomic_fetch_or_explicit(object, operand, memory_order_seq_cst)
#	define atomic_fetch_sub(object, operand)                                  \
		atomic_fetch_sub_explicit(object, operand, memory_order_seq_cst)
#	define atomic_fetch_xor(object, operand)                                  \
		atomic_fetch_xor_explicit(object, operand, memory_order_seq_cst)
#	define atomic_load(object)                                                \
		atomic_load_explicit(object, memory_order_seq_cst)
#	define atomic_store(object, desired)                                      \
		atomic_store_explicit(object, desired, memory_order_seq_cst)
#endif

typedef _Atomic(_Bool)              atomic_bool;
typedef _Atomic(char)               atomic_char;
typedef _Atomic(signed char)        atomic_schar;
typedef _Atomic(unsigned char)      atomic_uchar;
typedef _Atomic(short)              atomic_short;
typedef _Atomic(unsigned short)     atomic_ushort;
typedef _Atomic(int)                atomic_int;
typedef _Atomic(unsigned int)       atomic_uint;
typedef _Atomic(long)               atomic_long;
typedef _Atomic(unsigned long)      atomic_ulong;
typedef _Atomic(long long)          atomic_llong;
typedef _Atomic(unsigned long long) atomic_ullong;
typedef _Atomic(int_least8_t)       atomic_int_least8_t;
typedef _Atomic(uint_least8_t)      atomic_uint_least8_t;
typedef _Atomic(int_least16_t)      atomic_int_least16_t;
typedef _Atomic(uint_least16_t)     atomic_uint_least16_t;
typedef _Atomic(int_least32_t)      atomic_int_least32_t;
typedef _Atomic(uint_least32_t)     atomic_uint_least32_t;
typedef _Atomic(int_least64_t)      atomic_int_least64_t;
typedef _Atomic(uint_least64_t)     atomic_uint_least64_t;
typedef _Atomic(int_fast8_t)        atomic_int_fast8_t;
typedef _Atomic(uint_fast8_t)       atomic_uint_fast8_t;
typedef _Atomic(int_fast16_t)       atomic_int_fast16_t;
typedef _Atomic(uint_fast16_t)      atomic_uint_fast16_t;
typedef _Atomic(int_fast32_t)       atomic_int_fast32_t;
typedef _Atomic(uint_fast32_t)      atomic_uint_fast32_t;
typedef _Atomic(int_fast64_t)       atomic_int_fast64_t;
typedef _Atomic(uint_fast64_t)      atomic_uint_fast64_t;
typedef _Atomic(intptr_t)           atomic_intptr_t;
typedef _Atomic(uintptr_t)          atomic_uintptr_t;
typedef _Atomic(size_t)             atomic_size_t;
typedef _Atomic(ptrdiff_t)          atomic_ptrdiff_t;


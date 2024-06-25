// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef __CDEFS_H__
#define __CDEFS_H__

/*
 * Testing against Clang-specific extensions.
 */
#ifndef __has_attribute
#	define __has_attribute(x) 0
#endif
#ifndef __has_extension
#	define __has_extension __has_feature
#endif
#ifndef __has_feature
#	define __has_feature(x) 0
#endif
#ifndef __has_include
#	define __has_include(x) 0
#endif
#ifndef __has_builtin
#	define __has_builtin(x) 0
#endif

/// Helper to use C++ in headers only in C++ mode.
#ifdef __cplusplus
#	define __if_cxx(x) x
#	define __if_c(x)
#else
#	define __if_cxx(x)
#	define __if_c(x) x
#endif

/// Allow the C99 spelling of bool in C++
#ifdef __cplusplus
using _Bool = bool;
#endif

#if defined(__cplusplus)
#	define __BEGIN_DECLS                                                      \
		extern "C"                                                             \
		{
#	define __END_DECLS }
#	define __DECL extern "C"
#else
#	define __BEGIN_DECLS
#	define __END_DECLS
#	define __DECL
#endif

#define __weak_symbol __attribute__((weak))
#define __dead2 __attribute__((noreturn))
#define __pure2 __attribute__((const))
#define __noinline __attribute__((noinline))
#define __always_inline __attribute__((always_inline))
#define __unused __attribute__((unused))
#define __used __attribute__((used))
#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))
#define __section(x) __attribute__((section(x)))
#define __alloc_size(x) __attribute__((alloc_size(x)))
#define __alloc_align(x) __attribute__((alloc_align(x)))
#define __cheri_callback __attribute__((cheri_ccallback))
#if __has_attribute(cheriot_minimum_stack)
#	define __cheriot_minimum_stack(x) __attribute__((cheriot_minimum_stack(x)))
#else
#	warning                                                                    \
	  "cheriot_minimum_stack attribute not supported, please update your compiler"
#	define __cheriot_minimum_stack(x)
#endif
// When running clang-tidy, we use the same compile flags for everything and so
// will get errors about things being defined in the wrong compartment, so
// define away the compartment name and pretend everything is local for now.
#ifdef CLANG_TIDY
#	define __cheri_compartment(x)
#else
#	define __cheri_compartment(x) __attribute__((cheri_compartment(x)))
#endif
#define __cheri_libcall __attribute__((cheri_libcall))

#define offsetof(a, b) __builtin_offsetof(a, b)

#define __predict_true(exp) __builtin_expect((exp), 1)
#define __predict_false(exp) __builtin_expect((exp), 0)

#define __XSTRING(a) __STRING(a)
#define __STRING(a) #a
/// Inner implementation for `__pragma`
#define __pragma_helper(x) _Pragma(#x)
/// Helper that allows pragma strings to be constructed with concatenation
#define __pragma(x) __pragma_helper(x)

#ifdef __clang__
/**
 * Helper that pushes the diagnostic stack and adds the argument (specified as
 * a string, of the form "-W{warning}") to the ignored list.
 */
#	define __clang_ignored_warning_push(x)                                    \
		_Pragma("clang diagnostics push") __pragma(clang diagnostic ignored x)
/**
 * Undoes the most recent `__clang_ignored_warning_push`.
 */
#	define __clang_ignored_warning_pop() _Pragma("clang diagnostics pop")
#else
#	define __clang_ignored_warning_push(x)
#	define __clang_ignored_warning_pop()
#endif

/**
 * Define the symbol for the libcall that the compiler will expand the `strlen`
 * builtin to.  This builtin is used internally in libc++ (and possibly in
 * other places) to avoid the namespace pollution from including `string.h` but
 * is either constant folded in the front end or expanded to a libcall.
 */
unsigned __builtin_strlen(const char *str) __asm__("_Z6strlenPKc");

#endif // _CDEFS_H_

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

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
#if defined(CLANG_TIDY) || defined(__CHERIOT_BAREMETAL__)
#	define __cheri_compartment(x)
#	define __cheriot_compartment(x)
#else
#	if __has_attribute(cheriot_compartment)
#		define __cheri_compartment(x) __attribute__((cheriot_compartment(x)))
#		define __cheriot_compartment(x) __attribute__((cheriot_compartment(x)))
#	else
#		define __cheri_compartment(x) __attribute__((cheri_compartment(x)))
#		define __cheriot_compartment(x) __attribute__((cheri_compartment(x)))
#	endif
#endif

// Define the CHERIoT calling-convention attributes macros to nothing if we're
// targeting bare metal and to the correct attributes if we're targeting the
// RTOS.
#ifdef __CHERIOT_BAREMETAL__
#	define __cheri_libcall
#	define __cheriot_libcall
#	define __cheri_callback
#	define __cheriot_callback
#else
#	if __has_attribute(cheriot_libcall)
#		define __cheri_libcall __attribute__((cheriot_libcall))
#		define __cheriot_libcall __attribute__((cheriot_libcall))
#	else
#		define __cheri_libcall __attribute__((cheri_libcall))
#		define __cheriot_libcall __attribute__((cheri_libcall))
#	endif
#	if __has_attribute(cheriot_libcall)
#		define __cheri_callback __attribute__((cheriot_ccallback))
#		define __cheriot_callback __attribute__((cheriot_ccallback))
#	else
#		define __cheri_callback __attribute__((cheri_ccallback))
#		define __cheriot_callback __attribute__((cheri_ccallback))
#	endif
#endif

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

#if !defined(CLANG_TIDY) && !__has_builtin(__builtin_cheri_top_get)
#	error Your compiler is too old for this version of CHERIoT RTOS, please upgrade to a newer version
#endif

#if (defined(__CHERIOT__) && __CHERIOT__ < 20250113) ||                        \
  (defined(__CHERIOT_BAREMETAL__) && __CHERIOT_BAREMETAL__ < 20250113)
#	error Your compiler is too old for this version of CHERIoT RTOS, please upgrade to a newer version
#endif

#define CHERIOT_VERSION_TRIPLE(major, minor, patch)                            \
	((major * 10000) + (minor * 100) + (patch))

/**
 * Helper for declaring standard library functions that should be exported
 * as __cheri_libcall, but which should not have their names mangled. This
 * is needed to enable LLVM optimizations to trigger on standard library
 * functions.
 */
#define CHERIOT_DECLARE_STANDARD_LIBCALL(name, ret, ...)                       \
	__cheri_libcall ret name(__VA_ARGS__) __asm__(#name);

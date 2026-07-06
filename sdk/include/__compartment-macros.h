// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <__cheri_sealed.h>
#include <cdefs.h>
#include <stdbool.h>

/**
 * \file
 *
 * Helper macros used by compartment-macros.h.
 *
 * Do not use any of the macros in this file directly.
 *
 * These are separated to make the documentation of compartment-macros.h
 * cleaner.
 */

/**
 * \internal
 *
 * Helper macro for MMIO and pre-shared object imports, should not be used
 * directly.
 */
#define IMPORT_CAPABILITY_WITH_PERMISSIONS_HELPER(type,                        \
                                                  name,                        \
                                                  prefix,                      \
                                                  mangledName,                 \
                                                  permitLoad,                  \
                                                  permitStore,                 \
                                                  permitLoadStoreCapabilities, \
                                                  permitLoadMutable,           \
                                                  permitLoadGlobal)            \
	({                                                                         \
		type *ret; /* NOLINT(bugprone-macro-parentheses) */                    \
		__asm(".ifndef " mangledName "\n"                                      \
		      "  .type     " mangledName ",@object\n"                          \
		      "  .section  .compartment_imports." #name                        \
		      ",\"awG\",@progbits," mangledName ",comdat\n"                    \
		      "  .globl    " mangledName "\n"                                  \
		      "  .p2align  3\n" mangledName ":\n"                              \
		      "  .word " #prefix #name "\n"                                    \
		      "  .word " #prefix #name "_end - " #prefix #name " + %c1\n"      \
		      "  .size " mangledName ", 8\n"                                   \
		      " .previous\n"                                                   \
		      ".endif\n"                                                       \
		      "1:"                                                             \
		      "  auipcc  %0,"                                                  \
		      "      %%cheriot_compartment_hi(" mangledName ")\n"              \
		      "  clc     %0, %%cheriot_compartment_lo_i(1b)(%0)\n"             \
		      : "=C"(ret)                                                      \
		      : "i"(((permitLoad) ? (1 << 31) : 0) +                           \
		            ((permitStore) ? (1 << 30) : 0) +                          \
		            ((permitLoadStoreCapabilities) ? (1 << 29) : 0) +          \
		            ((permitLoadMutable) ? (1 << 28) : 0) +                    \
		            ((permitLoadGlobal) ? (1 << 27) : 0)));                    \
		ret;                                                                   \
	})

// NOLINTBEGIN
#define CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG_true(flag) flag
#define CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG_1(flag) flag
#define CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG_false(flag) ""
#define CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG_0(flag) ""

#define CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG(cond, flag)                     \
	CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG_##cond(flag)
// NOLINTEND

#define CHERIOT_COMPARTMENT_MACROS_JOIN_FLAGS(pL, pS, pC, pM, pG)              \
	CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG(pL, "R")                            \
	CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG(pS, "W")                            \
	CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG(pC, "c")                            \
	CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG(pM, "m")                            \
	CHERIOT_COMPARTMENT_MACROS_PERMIT_FLAG(pG, "g")

#define CHERIOT_COMPARTMENT_MACROS_TOKEN_PASTE_INNER(x, y) x##y
#define CHERIOT_COMPARTMENT_MACROS_TOKEN_PASTE(x, y)                           \
	CHERIOT_COMPARTMENT_MACROS_TOKEN_PASTE_INNER(x, y)

#if defined(__has_attribute) && __has_attribute(cheriot_mmio)
/**
 * \internal
 *
 * Helper for `MMIO_CAPABILITY_WITH_PERMISSIONS_` that forces evaluation of the
 * `__COUNTER__` macro.
 */
#	define MMIO_CAPABILITY_WITH_PERMISSIONS_INNER(                            \
	  type,                                                                    \
	  name,                                                                    \
	  permitLoad,                                                              \
	  permitStore,                                                             \
	  permitLoadStoreCapabilities,                                             \
	  permitLoadMutable,                                                       \
	  permitLoadGlobal,                                                        \
	  cValue)                                                                  \
		({                                                                     \
			/* NOLINTBEGIN(bugprone-macro-parentheses) */                      \
			_Static_assert(permitLoad || permitStore,                          \
			               "Importing an MMIO capability with no permissions " \
			               "is not allowed.");                                 \
			_Pragma("GCC diagnostic push")                                     \
			  _Pragma("GCC diagnostic ignored \"-Wundefined-internal\"")       \
			    __attribute__((cheri_no_subobject_bounds))                     \
			    __attribute__((cheriot_mmio(                                   \
			      #name,                                                       \
			      CHERIOT_COMPARTMENT_MACROS_JOIN_FLAGS(                       \
			        permitLoad,                                                \
			        permitStore,                                               \
			        permitLoadStoreCapabilities,                               \
			        permitLoadMutable,                                         \
			        permitLoadGlobal)))) volatile extern char cValue;          \
			(type *)&cValue;                                                   \
			_Pragma("GCC diagnostic pop")                                      \
			/* NOLINTEND(bugprone-macro-parentheses) */                        \
		})
#else
/**
 * \internal
 *
 * Helper macro, should not be used directly.
 */
#	define MMIO_CAPABILITY_WITH_PERMISSIONS_HELPER(                           \
	  type,                                                                    \
	  name,                                                                    \
	  mangledName,                                                             \
	  permitLoad,                                                              \
	  permitStore,                                                             \
	  permitLoadStoreCapabilities,                                             \
	  permitLoadMutable,                                                       \
	  permitLoadGlobal)                                                        \
		IMPORT_CAPABILITY_WITH_PERMISSIONS_HELPER(type,                        \
		                                          name,                        \
		                                          __export_mem_,               \
		                                          mangledName,                 \
		                                          permitLoad,                  \
		                                          permitStore,                 \
		                                          permitLoadStoreCapabilities, \
		                                          permitLoadMutable,           \
		                                          permitLoadGlobal)

#endif

#if defined(__has_attribute) && __has_attribute(cheriot_shared_object)
/**
 * \internal
 *
 * Helper for implementing `SHARED_OBJECT_WITH_PERMISSIONS`, expanding the
 * definition of `__COUNTER__`, passed as the final macro parameter.
 */
#	define SHARED_OBJECT_WITH_PERMISSIONS_INNER(type,                         \
	                                             name,                         \
	                                             permitLoad,                   \
	                                             permitStore,                  \
	                                             permitLoadStoreCapabilities,  \
	                                             permitLoadMutable,            \
	                                             permitLoadGlobal,             \
	                                             cValue)                       \
		({                                                                     \
			/* NOLINTBEGIN(bugprone-macro-parentheses) */                      \
			_Static_assert(                                                    \
			  permitLoad || permitStore,                                       \
			  "Importing a shared object capability with no permissions "      \
			  "is not allowed.");                                              \
			_Pragma("GCC diagnostic push")                                     \
			  _Pragma("GCC diagnostic ignored \"-Wundefined-internal\"")       \
			    __attribute__((cheri_no_subobject_bounds))                     \
			    __attribute__((cheriot_shared_object(                          \
			      #name,                                                       \
			      CHERIOT_COMPARTMENT_MACROS_JOIN_FLAGS(                       \
			        permitLoad,                                                \
			        permitStore,                                               \
			        permitLoadStoreCapabilities,                               \
			        permitLoadMutable,                                         \
			        permitLoadGlobal)))) extern char cValue;                   \
			(type *)&cValue;                                                   \
			_Pragma("GCC diagnostic pop")                                      \
			/* NOLINTEND(bugprone-macro-parentheses) */                        \
		})
#endif

/**
 * \internal
 *
 * Helper macro, used by `STATIC_SEALING_TYPE`.  Do not use this
 * directly, it exists to avoid error-prone copying and pasting
 * of the mangled name for a static sealing type.
 */
#define CHERIOT_EMIT_STATIC_SEALING_TYPE(name)                                 \
	({                                                                         \
		TokenKey ret; /* NOLINT(bugprone-macro-parentheses) */                 \
		__asm(                                                                 \
		  ".ifndef __import." name "\n"                                        \
		  "  .type     __import." name ",@object\n"                            \
		  "  .section  .compartment_imports." name ",\"awG\",@progbits," name  \
		  ",comdat\n"                                                          \
		  "  .globl    __import." name "\n"                                    \
		  "  .p2align  3\n"                                                    \
		  "__import." name ":\n"                                               \
		  "  .word __export." name "\n"                                        \
		  "  .word 0\n"                                                        \
		  " .previous\n"                                                       \
		  ".endif\n"                                                           \
		  ".ifndef __export." name "\n"                                        \
		  "  .type     __export." name ",@object\n"                            \
		  "  .section  .compartment_exports." name ",\"awG\",@progbits," name  \
		  ",comdat\n"                                                          \
		  "  .globl    __export." name "\n"                                    \
		  "  .p2align  2\n"                                                    \
		  "__export." name ":\n"                                               \
		  "  .half 0\n" /* function start and stack size initialised to 0 */   \
		  "  .byte 0\n"                                                        \
		  "  .byte 0b100000\n" /* Set the flag that indicates that this is a   \
		                          sealing key. */                              \
		  "  .size __export." name ", 4\n"                                     \
		  " .previous\n"                                                       \
		  ".endif\n"                                                           \
		  "1:\n"                                                               \
		  "  auipcc  %0, %%cheriot_compartment_hi(__import." name ")\n"        \
		  "  clc     %0, %%cheriot_compartment_lo_i(1b)(%0)\n"                 \
		  : "=C"(ret));                                                        \
		ret;                                                                   \
	})

/**
 * \internal
 *
 * Helper macro that evaluates to the compartment of the current
 * compilation unit, as a string.
 */
#define COMPARTMENT_NAME_STRING __XSTRING(__CHERI_COMPARTMENT__)

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cdefs.h>

/**
 * Provide a capability of the type `volatile type *` referring to the MMIO
 * region exported in the linker script with `name` as its name.  This macro
 * can be used only in code (it cannot be used to initialise a global).
 */
#define MMIO_CAPABILITY(type, name)                                            \
	({                                                                         \
		volatile type *ret; /* NOLINT(bugprone-macro-parentheses) */           \
		__asm(".ifndef __import_mem_" #name "\n"                               \
		      "  .type     __import_mem_" #name ",@object\n"                   \
		      "  .section  .compartment_imports." #name                        \
		      ",\"awG\",@progbits," #name ",comdat\n"                          \
		      "  .globl    __import_mem_" #name "\n"                           \
		      "  .p2align  3\n"                                                \
		      "__import_mem_" #name ":\n"                                      \
		      "  .word __export_mem_" #name "\n"                               \
		      "  .word __export_mem_" #name "_end - __export_mem_" #name "\n"  \
		      "  .size __import_mem_" #name ", 8\n"                            \
		      " .previous\n"                                                   \
		      ".endif\n"                                                       \
		      "1:"                                                             \
		      "  auipcc  %0,"                                                  \
		      "      %%cheri_compartment_pccrel_hi(__import_mem_" #name ")\n"  \
		      "  clc     %0, %%cheri_compartment_pccrel_lo(1b)(%0)\n"          \
		      : "=C"(ret));                                                    \
		ret;                                                                   \
	})

/**
 * Macro to test whether a device with a specific name exists in the board
 * definition for the current target.
 */
#define DEVICE_EXISTS(x) defined(DEVICE_EXISTS_##x)

#define SEALING_CAP()                                                          \
	({                                                                         \
		void *ret;                                                             \
		__asm("1:	"                                                            \
		      "	auipcc		%0, %%cheri_compartment_pccrel_hi(__sealingkey)\n"     \
		      "	clc			%0, %%cheri_compartment_pccrel_lo(1b)(%0)\n"             \
		      : "=C"(ret));                                                    \
		ret;                                                                   \
	})

/**
 * Helper macro, used by `STATIC_SEALING_TYPE`.  Do not use this directly, it
 * exists to avoid error-prone copying and pasting of the mangled name for a
 * static sealing type.
 */
#define CHERIOT_EMIT_STATIC_SEALING_TYPE(name)                                 \
	({                                                                         \
		SKey ret; /* NOLINT(bugprone-macro-parentheses) */                     \
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
		  "  auipcc  %0, %%cheri_compartment_pccrel_hi(__import." name ")\n"   \
		  "  clc     %0, %%cheri_compartment_pccrel_lo(1b)(%0)\n"              \
		  : "=C"(ret));                                                        \
		ret;                                                                   \
	})

/**
 * Helper macro that evaluates to the compartment of the current compilation
 * unit, as a string.
 */
#define COMPARTMENT_NAME_STRING __XSTRING(__CHERI_COMPARTMENT__)

/**
 * Macro that evaluates to a static sealing type that is local to this
 * compartment.
 */
#define STATIC_SEALING_TYPE(name)                                              \
	CHERIOT_EMIT_STATIC_SEALING_TYPE("sealing_type." COMPARTMENT_NAME_STRING   \
	                                 "." #name)

/**
 * Forward-declare a static sealed object.  This declares an object of type
 * `type` that can be referenced with the `STATIC_SEALED_VALUE` macro using
 * `name`.  The pointer returned by the latter macro will be sealed with the
 * sealing key exported from `compartment` as `keyName` with the
 * `STATIC_SEALING_TYPE` macro.
 *
 * The object created with this macro can be accessed only by code that has
 * access to the sealing key.
 */
#define DECLARE_STATIC_SEALED_VALUE(type, compartment, keyName, name)          \
	struct __##name##_type; /* NOLINT(bugprone-macro-parentheses) */           \
	extern __if_cxx("C") struct __##name##_type                                \
	  name; /* NOLINT(bugprone-macro-parentheses) */

/**
 * Define a static sealed object.  This creates an object of type `type`,
 * initialised with `initialiser`, that can be referenced with the
 * `STATIC_SEALED_VALUE` macro using `name`.  The pointer returned by the
 * latter macro will be sealed with the sealing key exported from `compartment`
 * as `keyName` with the `STATIC_SEALING_TYPE` macro.
 *
 * The object created with this macro can be accessed only by code that has
 * access to the sealing key.
 */
#define DEFINE_STATIC_SEALED_VALUE(                                            \
  type, compartment, keyName, name, initialiser, ...)                          \
	extern __if_cxx("C") int __sealing_key_##compartment##_##keyName __asm(    \
	  "__export.sealing_type." #compartment "." #keyName);                     \
	__attribute__((section(".sealed_objects"), used))                          \
	__if_cxx(inline) struct __##name##_type                                    \
	{                                                                          \
		uint32_t key;                                                          \
		uint32_t padding;                                                      \
		type     body;                                                         \
	} name = /* NOLINT(bugprone-macro-parentheses) */                          \
	  {(uint32_t)&__sealing_key_##compartment##_##keyName,                     \
	   0,                                                                      \
	   initialiser,                                                            \
	   ##__VA_ARGS__}

/**
 * Returns a sealed capability to the named object created with
 * `DECLARE_STATIC_SEALED_VALUE`.
 */
#define STATIC_SEALED_VALUE(name)                                              \
	({                                                                         \
		struct SObjStruct *ret; /* NOLINT(bugprone-macro-parentheses) */       \
		__asm(                                                                 \
		  ".ifndef __import.sealed_object." #name "\n"                         \
		  "  .type     __import.sealed_object." #name ",@object\n"             \
		  "  .section  .compartment_imports." #name                            \
		  ",\"awG\",@progbits," #name ",comdat\n"                              \
		  "  .globl    __import.sealed_object." #name "\n"                     \
		  "  .p2align  3\n"                                                    \
		  "__import.sealed_object." #name ":\n"                                \
		  "  .word " #name "\n"                                                \
		  "  .word %c1\n"                                                      \
		  "  .size __import.sealed_object." #name ", 8\n"                      \
		  " .previous\n"                                                       \
		  ".endif\n"                                                           \
		  "1:"                                                                 \
		  "  auipcc  %0,"                                                      \
		  "      %%cheri_compartment_pccrel_hi(__import.sealed_object." #name  \
		  ")\n"                                                                \
		  "  clc     %0, %%cheri_compartment_pccrel_lo(1b)(%0)\n"              \
		  : "=C"(ret)                                                          \
		  : "i"(sizeof(__typeof__(name))));                                    \
		ret;                                                                   \
	})

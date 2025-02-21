// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <__cheri_sealed.h>
#include <cdefs.h>
#include <stdbool.h>

/**
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
                                                  permitLoadMutable)           \
	({                                                                         \
		type *ret; /* NOLINT(bugprone-macro-parentheses) */                    \
		__asm(".ifndef " mangledName "\n"                                      \
		      "  .type     " mangledName ",@object\n"                          \
		      "  .section  .compartment_imports." #name                        \
		      ",\"awG\",@progbits," #name ",comdat\n"                          \
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
		            ((permitLoadMutable) ? (1 << 28) : 0)));                   \
		ret;                                                                   \
	})

/**
 * Helper macro, should not be used directly.
 */
#define MMIO_CAPABILITY_WITH_PERMISSIONS_HELPER(type,                          \
                                                name,                          \
                                                mangledName,                   \
                                                permitLoad,                    \
                                                permitStore,                   \
                                                permitLoadStoreCapabilities,   \
                                                permitLoadMutable)             \
	IMPORT_CAPABILITY_WITH_PERMISSIONS_HELPER(type,                            \
	                                          name,                            \
	                                          __export_mem_,                   \
	                                          mangledName,                     \
	                                          permitLoad,                      \
	                                          permitStore,                     \
	                                          permitLoadStoreCapabilities,     \
	                                          permitLoadMutable)

/**
 * Provide a capability of the type `volatile type *` referring to the MMIO
 * region exported in the linker script with `name` as its name.  This macro
 * can be used only in code (it cannot be used to initialise a global).
 *
 * The last arguments specify the set of permissions that this capability
 * holds.  MMIO capabilities are always global and without store local.  They
 * may optionally omit additional capabilities.
 */
#define MMIO_CAPABILITY_WITH_PERMISSIONS(type,                                 \
                                         name,                                 \
                                         permitLoad,                           \
                                         permitStore,                          \
                                         permitLoadStoreCapabilities,          \
                                         permitLoadMutable)                    \
	MMIO_CAPABILITY_WITH_PERMISSIONS_HELPER(                                   \
	  volatile type, /* NOLINT(bugprone-macro-parentheses) */                  \
	  name,                                                                    \
	  "__import_mem_" #name "_" #permitLoad "_" #permitStore                   \
	  "_" #permitLoadStoreCapabilities "_" #permitLoadMutable,                 \
	  permitLoad,                                                              \
	  permitStore,                                                             \
	  permitLoadStoreCapabilities,                                             \
	  permitLoadMutable)

/**
 * Provide a capability of the type `volatile type *` referring to the MMIO
 * region exported in the linker script with `name` as its name.  This macro
 * can be used only in code (it cannot be used to initialise a global).
 *
 * MMIO capabilities produced by this macro have load and store permissions but
 * cannot hold capabilities.  For richer permissions use
 * MMIO_CAPABILITY_WITH_PERMISSIONS.
 */
#define MMIO_CAPABILITY(type, name)                                            \
	MMIO_CAPABILITY_WITH_PERMISSIONS(type, name, true, true, false, false)

/**
 * Provide a capability of the type `type *` referring to the pre-shared object
 * with `name` as its name.  This macro can be used only in code (it cannot be
 * used to initialise a global).
 *
 * The last arguments specify the set of permissions that this capability
 * holds.  Pre-shared objects are always global and without store local.  They
 * may optionally omit additional permissions.
 */
#define SHARED_OBJECT_WITH_PERMISSIONS(type,                                   \
                                       name,                                   \
                                       permitLoad,                             \
                                       permitStore,                            \
                                       permitLoadStoreCapabilities,            \
                                       permitLoadMutable)                      \
	IMPORT_CAPABILITY_WITH_PERMISSIONS_HELPER(                                 \
	  type, /* NOLINT(bugprone-macro-parentheses) */                           \
	  name,                                                                    \
	  __cheriot_shared_object_,                                                \
	  "__import_cheriot_shared_object_" #name "_" #permitLoad "_" #permitStore \
	  "_" #permitLoadStoreCapabilities "_" #permitLoadMutable,                 \
	  permitLoad,                                                              \
	  permitStore,                                                             \
	  permitLoadStoreCapabilities,                                             \
	  permitLoadMutable)

/**
 * Provide a capability of the type `type *` referring to the pre-shared object
 * with `name` as its name.  This macro can be used only in code (it cannot be
 * used to initialise a global).
 *
 * Pre-shared object capabilities produced by this macro have load, store,
 * load-mutable, and load/store-capability permissions.  To define a reduced
 * set of permissions use `SHARED_OBJECT_WITH_PERMISSIONS`.
 */
#define SHARED_OBJECT(type, name)                                              \
	SHARED_OBJECT_WITH_PERMISSIONS(type, name, true, true, true, true)

/**
 * Macro to test whether a device with a specific name exists in the board
 * definition for the current target.
 */
#define DEVICE_EXISTS(x) defined(DEVICE_EXISTS_##x)

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
		  "  auipcc  %0, %%cheriot_compartment_hi(__import." name ")\n"        \
		  "  clc     %0, %%cheriot_compartment_lo_i(1b)(%0)\n"                 \
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
 *
 * Unlike `DECLARE_STATIC_SEALED_VALUE`, this allows the type that value should
 * be read as to be different to the type used to initialise it.  The type of
 * the value in `STATIC_SEALED_VALUE` will be `CHERI_SEALED(valueType*)`.  This
 * is useful for variable length arrays at the end of the structure.
 */
#define DECLARE_STATIC_SEALED_VALUE_EXPLICIT_TYPE(                             \
  type, valueType, compartment, keyName, name)                                 \
	/* Implementation detail: This declaration in the reserved namespace       \
	 * exists only so that __typeof__ can be used later to determine the       \
	 * original type.  This will be removed once the compiler understands      \
	 * static sealed objects natively. */                                      \
	extern valueType __sealed_type_placeholder_##name;                         \
	extern __if_cxx("C") struct __##name##_type                                \
	{                                                                          \
		uint32_t key;                                                          \
		uint32_t padding;                                                      \
		type     body;                                                         \
	}(name);                                                                   \
	/* Make sure the type that we're casting this to is not bigger than the    \
	 * value that we've emitted. */                                            \
	_Static_assert(sizeof(__sealed_type_placeholder_##name) <=                 \
	               sizeof((name).body))

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
	DECLARE_STATIC_SEALED_VALUE_EXPLICIT_TYPE(                                 \
	  type, type, compartment, keyName, name)

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
	__attribute__((section(".sealed_objects"), used)) __if_cxx(                \
	  inline) struct __##name##_type                                           \
	  name = /* NOLINT(bugprone-macro-parentheses) */                          \
	  {(uint32_t) & __sealing_key_##compartment##_##keyName,                   \
	   0,                                                                      \
	   {initialiser, ##__VA_ARGS__}}

/**
 * Helper macro that declares and defines a sealed value.
 *
 * Unlike `DECLARE_AND_DEFINE_STATIC_SEALED_VALUE`, this allows the type that
 * value should be read as to be different to the type used to initialise it.
 * The type of the value in `STATIC_SEALED_VALUE` will be
 * `CHERI_SEALED(valueType*)`.  This is useful for variable length arrays at
 * the end of the structure.
 */
#define DECLARE_AND_DEFINE_STATIC_SEALED_VALUE_EXPLICIT_TYPE(                  \
  type, valueType, compartment, keyName, name, initialiser, ...)               \
	DECLARE_STATIC_SEALED_VALUE_EXPLICIT_TYPE(                                 \
	  type,                                                                    \
	  valueType,                                                               \
	  compartment,                                                             \
	  keyName,                                                                 \
	  name); /* NOLINT(bugprone-macro-parentheses) */                          \
	DEFINE_STATIC_SEALED_VALUE(                                                \
	  type,                                                                    \
	  compartment,                                                             \
	  keyName,                                                                 \
	  name,                                                                    \
	  initialiser,                                                             \
	  ##__VA_ARGS__); /* NOLINT(bugprone-macro-parentheses) */

/**
 * Helper macro that declares and defines a sealed value.
 */
#define DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(                                \
  type, compartment, keyName, name, initialiser, ...)                          \
	DECLARE_AND_DEFINE_STATIC_SEALED_VALUE_EXPLICIT_TYPE(                      \
	  type, type, compartment, keyName, name, initialiser, ##__VA_ARGS__)

/**
 * Returns a sealed capability to the named object created with
 * `DECLARE_STATIC_SEALED_VALUE`.
 */
#define STATIC_SEALED_VALUE(name)                                              \
	({                                                                         \
		CHERI_SEALED(__typeof__(__sealed_type_placeholder_##name) *)           \
		ret; /* NOLINT(bugprone-macro-parentheses) */                          \
		__asm(".ifndef __import.sealed_object." #name "\n"                     \
		      "  .type     __import.sealed_object." #name ",@object\n"         \
		      "  .section  .compartment_imports." #name                        \
		      ",\"awG\",@progbits," #name ",comdat\n"                          \
		      "  .globl    __import.sealed_object." #name "\n"                 \
		      "  .p2align  3\n"                                                \
		      "__import.sealed_object." #name ":\n"                            \
		      "  .word " #name "\n"                                            \
		      "  .word %c1\n"                                                  \
		      "  .size __import.sealed_object." #name ", 8\n"                  \
		      " .previous\n"                                                   \
		      ".endif\n"                                                       \
		      "1:"                                                             \
		      "  auipcc  %0,"                                                  \
		      "      %%cheriot_compartment_hi(__import.sealed_object." #name   \
		      ")\n"                                                            \
		      "  clc     %0, %%cheriot_compartment_lo_i(1b)(%0)\n"             \
		      : "=C"(ret)                                                      \
		      : "i"(sizeof(__typeof__(name))));                                \
		ret;                                                                   \
	})

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cdefs.h>
#include <stdint.h>

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
	extern __if_cxx("C") int __sealing_key_##compartment##_##keyName asm(      \
	  "__export.sealing_type." #compartment "." #keyName);                     \
	__attribute__((section(".sealed_objects"), used)) struct                   \
	{                                                                          \
		uint32_t key;                                                          \
		uint32_t padding;                                                      \
		type     body;                                                         \
	} name; /* NOLINT(bugprone-macro-parentheses) */

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
  type, compartment, keyName, name, initialiser)                               \
	extern __if_cxx("C") int __sealing_key_##compartment##_##keyName asm(      \
	  "__export.sealing_type." #compartment "." #keyName);                     \
	__attribute__((section(".sealed_objects"), used)) struct                   \
	{                                                                          \
		uint32_t key;                                                          \
		uint32_t padding;                                                      \
		type     body;                                                         \
	} name = /* NOLINT(bugprone-macro-parentheses) */                          \
	  {(uint32_t)&__sealing_key_##compartment##_##keyName, 0, initialiser}

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

#ifdef __cplusplus
#	include <cheri.hh>
template<typename T>
static inline CHERI::Capability<T> compart_seal(T *in)
{
	void *key = SEALING_CAP();

	return CHERI::Capability{in}.seal(key);
}

template<typename T>
static inline CHERI::Capability<T> compart_unseal(T *in)
{
	void *key = SEALING_CAP();

	return CHERI::Capability{in}.unseal(key);
}

template<typename T>
static inline auto compart_unseal(void *in)
{
	return compart_unseal(static_cast<T *>(in));
}
#else
#	include <cheri-builtins.h>
static inline void *compart_seal(void *in)
{
	void *key = SEALING_CAP();

	return cseal(in, key);
}

static inline void *compart_unseal(void *in)
{
	void *key = SEALING_CAP();

	return cunseal(in, key);
}
#endif

/**
 * State for error handlers to use.
 *
 * Note: This structure should have the same layout as the register-save area.
 */
struct ErrorState
{
	/**
	 * The faulting program counter.  Error handlers may modify this but the
	 * result *must* point into the compartment's program counter capability.
	 * If it does not then the trusted stack will be unwound forcibly to the
	 * caller's compartment.
	 *
	 * Note: This is passed as an *untagged* capability.  This allows error
	 * handlers to inspect the bounds and permissions, but does not convey the
	 * rights.
	 */
	void *pcc;

	/**
	 * The register state where the fault occurred.  These may be modified by
	 * an error handler.
	 */
	void *registers[15];
#ifdef __cplusplus
	/**
	 * Templated method to get a reference to value of given
	 * CHERI::RegisterNumber.
	 *
	 * Static asserts that the register is one of the general purpose ones
	 * excluding CZR. This doesn't support getting PCC because in that case
	 * you can Use ErrorState.pcc directly.
	 */
	template<CHERI::RegisterNumber ARegisterNumber>
	[[nodiscard]] void *&get_register_value()
	{
		static_assert(ARegisterNumber > CHERI::RegisterNumber::CZR &&
		                ARegisterNumber <= CHERI::RegisterNumber::CA5,
		              "get_register_value: invalid RegisterNumber");
		return this->registers[static_cast<size_t>(ARegisterNumber) - 1];
	}

	/**
	 * Returns a pointer to the value of the given CHERI::RegisterNumber from
	 * this ErrorState.
	 *
	 * Will either select the appropriate index into ErrorState.registers
	 * accounting for the missing CZR, ErrorState.PCC, or if registerNumber is
	 * invalid or not contained in ErrorState then nullptr is returned.
	 */
	[[nodiscard]] void **
	get_register_value(CHERI::RegisterNumber registerNumber)
	{
		if (registerNumber > CHERI::RegisterNumber::CZR &&
		    registerNumber <= CHERI::RegisterNumber::CA5)
		{
			return &this->registers[static_cast<size_t>(registerNumber) - 1];
		}
		if (registerNumber == CHERI::RegisterNumber::PCC)
		{
			return &this->pcc;
		}
		return nullptr;
	}
#endif
};

/**
 * Valid return values from an error handler.
 */
enum ErrorRecoveryBehaviour
{
	/// Install the modified context.
	InstallContext,
	/// Unwind the trusted stack to the caller.
	ForceUnwind
};

__BEGIN_DECLS
/**
 * The error handler for the current compartment.  A compartment may choose to
 * implement this.  If not implemented then compartment faults will unwind the
 * trusted stack.
 */
__attribute__((
  section(".compartment_error_handler"))) enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame,
                          size_t             mcause,
                          size_t             mtval);
__END_DECLS

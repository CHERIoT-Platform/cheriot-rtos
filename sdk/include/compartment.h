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

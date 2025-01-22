// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <__cheri_sealed.h>
#include <compartment-macros.h>
#include <stddef.h>
#include <stdint.h>

struct Timeout;

/**
 * The complete set of architectural permissions.
 */
enum CHERIPermission
{
	/**
	 * Capability refers to global memory (this capability may be stored
	 * anywhere).
	 */
	CheriPermissionGlobal = 0,
	/**
	 * Global capabilities can be loaded through this capability.  Without
	 *  this permission, any capability loaded via this capability will
	 *  have `Global` and `LoadGlobal` removed.
	 */
	CheriPermissionLoadGlobal = 1,
	/**
	 * Capability may be used to store.  Any store via a capability without
	 * this permission will trap.
	 */
	CheriPermissionStore = 2,
	/**
	 * Capabilities with store permission may be loaded through this
	 * capability.  Without this, any loaded capability will have
	 * `LoadMutable` and `Store` removed.
	 */
	CheriPermissionLoadMutable = 3,
	/**
	 * This capability may be used to store capabilities that do not have
	 * `Global` permission.
	 */
	CheriPermissionStoreLocal = 4,
	/**
	 * This capability can be used to load.
	 */
	CheriPermissionLoad = 5,
	/**
	 * Any load and store permissions on this capability convey the right to
	 * load or store capabilities in addition to data.
	 */
	CheriPermissionLoadStoreCapability = 6,
	/**
	 * If installed as the program counter capability, running code may
	 * access privileged system registers.
	 */
	CheriPermissionAccessSystemRegisters = 7,
	/**
	 * This capability may be used as a jump target and used to execute
	 * instructions.
	 */
	CheriPermissionExecute = 8,
	/**
	 * This capability may be used to unseal other capabilities.  The
	 * 'address' range is in the sealing type namespace and not in the
	 * memory namespace.
	 */
	CheriPermissionUnseal = 9,
	/**
	 * This capability may be used to seal other capabilities.  The
	 * 'address' range is in the sealing type namespace and not in the
	 * memory namespace.
	 */
	CheriPermissionSeal = 10,
	/**
	 * Software defined permission bit, no architectural meaning.
	 */
	CheriPermissionUser0 = 11
};

/**
 * The codes used in the cause field of the mtval CSR when the processor
 * takes a CHERI exception.
 */
enum CHERICauseCode
{
	/**
	 * No exception. This value is passed to the error handler after a
	 * forced unwind in a called compartment.
	 */
	CheriCauseCodeNone = 0,
	/**
	 * Attempted to use a capability outside its bounds.
	 */
	CheriCauseCodeBoundsViolation = 1,
	/**
	 * Attempted to use an untagged capability to authorize something.
	 */
	CheriCauseCodeTagViolation = 2,
	/**
	 * Attempted to use a sealed capability to authorize something.
	 */
	CheriCauseCodeSealViolation = 3,
	/**
	 * Attempted to jump to a capability without `Permission::Execute`.
	 */
	CheriCauseCodePermitExecuteViolation = 0x11,
	/**
	 * Attempted to load via a capability without `Permission::Load`.
	 */
	CheriCauseCodePermitLoadViolation = 0x12,
	/**
	 * Attempted to store via a capability without `Permission::Store`.
	 */
	CheriCauseCodePermitStoreViolation = 0x13,
	/**
	 * Attempted to store a tagged capability via a capability without
	 * `Permission::LoadStoreCapability`.
	 */
	CheriCauseCodePermitStoreCapabilityViolation = 0x15,
	/**
	 * Attempted to store a tagged capability without `Permission::Global`
	 * via capability without `Permission::StoreLocal`.
	 */
	CheriCauseCodePermitStoreLocalCapabilityViolation = 0x16,
	/**
	 * Attempted to access a restricted CSR or SCR with PCC without
	 * `Permission::AccessSystemRegisters`.
	 */
	CheriCauseCodePermitAccessSystemRegistersViolation = 0x18,
	/**
	 * Used to represent a value that has no valid meaning in hardware.
	 */
	CheriCauseCodeInvalid = -1
};

/**
 * Register numbers as reported in cap idx field of  `mtval` CSR when
 * a CHERI exception is taken. Values less than 32 refer to general
 * purpose registers and others to SCRs (of these, only PCC can actually
 * cause an exception).
 */
enum CHERIRegisterNumber
{
	/**
	 * The zero register, which always contains the `NULL` capability.
	 */
	CheriRegisterNumberCzr = 0x0,
	/**
	 * `$c1` / `$cra` used by the ABI as the return address.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCra = 0x1,
	/**
	 * `$c2` / `$csp` used by the ABI as the stack pointer.
	 * Preserved across calls.
	 */
	CheriRegisterNumberCsp = 0x2,
	/**
	 * `$c3` / `$cgp` used by the ABI as the global pointer.
	 * Not allocatable by the compiler, set by the switcher on compartment
	 * entry.
	 */
	CheriRegisterNumberCgp = 0x3,
	/**
	 * `$c4` / `$ctp` used by the ABI as the thread pointer.
	 * Currently unused by the compiler.
	 * Not preserved across compartment calls.
	 */
	CheriRegisterNumberCtp = 0x4,
	/**
	 * `$c5` / `$ct0` used by the ABI as temporary register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCT0 = 0x5,
	/**
	 * `$c6` / `$ct1` used by the ABI as temporary register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCT1 = 0x6,
	/**
	 * `$c7` / `$ct2` used by the ABI as temporary register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCT2 = 0x7,
	/**
	 * `$c8` / `$cs0` used by the ABI as a callee-saved register.
	 * Preserved across calls.
	 */
	CheriRegisterNumberCS0 = 0x8,
	/**
	 * `$c9` / `$cs1` used by the ABI as a callee-saved register.
	 * Preserved across calls.
	 */
	CheriRegisterNumberCS1 = 0x9,
	/**
	 * `$c10` / `$ca0` used by the ABI as an argument register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCA0 = 0xa,
	/**
	 * `$c11` / `$ca1` used by the ABI as an argument register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCA1 = 0xb,
	/**
	 * `$c12` / `$ca2` used by the ABI as an argument register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCA2 = 0xc,
	/**
	 * `$c13` / `$ca3` used by the ABI as an argument register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCA3 = 0xd,
	/**
	 * `$c14` / `$ca4` used by the ABI as an argument register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCA4 = 0xe,
	/**
	 * `$c15` / `$ca5` used by the ABI as an argument register.
	 * Not preserved across calls.
	 */
	CheriRegisterNumberCA5 = 0xf,
	/**
	 * The Program Counter Capability.
	 *
	 * Special capability register used to authorize instruction fetch. The
	 * address is that of the faulting instruction. Also used for accessing
	 * read-only globals.
	 */
	CheriRegisterNumberPcc = 0x20,
	/**
	 * Machine-mode Trap Code Capability.
	 *
	 * Special capability register that
	 * is installed in PCC when the CPU takes a trap. The address has the
	 * same semantics as the RISC-V `mtvec` CSR. Only accessible when PCC
	 * has the AccessSystemRegisters permission.
	 */
	CheriRegisterNumberMtcc = 0x3c,
	/**
	 * Machine-mode Tusted Data Capability.
	 *
	 * Special capability register that contains the memory root capability
	 * on boot. Only accessible when PCC has the AccessSystemRegisters
	 * permission.  Use by the RTOS to store a capability to the trusted
	 * stack.
	 */
	CheriRegisterNumberMtdc = 0x3d,
	/**
	 * Machine-mode Scratch Capability. Special capabiltiy register that
	 * contains the sealing root capability on boot. Only accessible when
	 * PCC has the AccessSystemRegisters permission.
	 */
	CheriRegisterNumberMScratchC = 0x3e,
	/**
	 * Machine-mode Exception Program Counter Capability. Special capability
	 * register that contains the PCC of the faulting instruction on trap.
	 * The address has the same semantics as the RISC-V `mepc` CSR. Only
	 * accessible when PCC has the AccessSystemRegisters permission.
	 */
	CheriRegisterNumberMepcc = 0x3f,
	/**
	 * Indicates a value that is not used by the hardware to refer to a
	 * register.
	 */
	CheriRegisterNumberInvalid = -1
};

/**
 * Sealing types.
 */
enum CHERISealingType
{
	/**
	 * 0 represents unsealed.
	 */
	CheriSealTypeUnsealed = 0,

	/**
	 * Sentry that inherits interrupt status.
	 */
	CheriSealTypeSentryInheriting,

	/**
	 * Sentry that disables interrupts on calls.
	 */
	CheriSealTypeSentryDisabling,

	/**
	 * Sentry that enables interrupts on calls.
	 */
	CheriSealTypeSentryEnabling,

	/**
	 * Return sentry that disables interrupts on return
	 */
	CheriSealTypeReturnSentryDisabling,

	/**
	 * Return sentry that enables interrupts on return
	 */
	CheriSealTypeReturnSentryEnabling,

	/**
	 * Marker for the first sealing type that's valid for data capabilities.
	 */
	CheriSealTypeFirstDataSealingType = 9,

	/**
	 * The sealing type used for sealed export table entries.
	 *
	 * This is RTOS- and not CHERIoT-specific.
	 */
	CheriSealTypeSealedImportTableEntries = CheriSealTypeFirstDataSealingType,

	/**
	 * The compartment switcher has a sealing type for the trusted stack.
	 *
	 * This must be the second data sealing type so that we can also permit
	 * the switcher to unseal sentries and export table entries.
	 *
	 * This is RTOS- and not CHERIoT-specific.
	 */
	CheriSealTypeSealedTrustedStacks,

	/**
	 * The allocator has a sealing type for the software sealing mechanism
	 * with dynamically allocated objects.
	 *
	 * This is RTOS- and not CHERIoT-specific.
	 */
	CheriSealTypeAllocator,

	/**
	 * The loader reserves a sealing type for the software sealing
	 * mechanism.  The permit-unseal capability for this is destroyed after
	 * the loader has run, which guarantees that anything sealed with this
	 * type was present in the original firmware image.  The token library
	 * has the only permit-unseal capability for this type.
	 *
	 * This is RTOS- and not CHERIoT-specific.
	 */
	CheriSealTypeStaticToken,

	/**
	 * The first sealing key that is reserved for use by the allocator's
	 * software sealing mechanism and used for static sealing types,
	 *
	 * Architecturally, this is the smallest non-interpreted sealing type.
	 */
	CheriSealTypeFirstStaticSoftware = 16,

	/**
	 * The first sealing key in the space that the allocator will
	 * dynamically allocate for sealing types.
	 *
	 * This is RTOS- and not CHERIoT-specific.
	 */
	CheriSealTypeFirstDynamicSoftware = 0x1000000
};

/**
 * Checks that `ptr` is valid, unsealed, has at least `rawPermissions`, and has
 * at least `space` bytes after the current offset.
 *
 * If the permissions do not include Global and `checkStackNeeded` is `false`,
 * then this will also check that the capability does not point to the current
 * thread's stack.
 *
 * To reduce code size, this function is provided as part of the compartment
 * helper library.
 */
bool __cheri_libcall check_pointer(const void *ptr,
                                   size_t      space,
                                   uint32_t    rawPermissions,
                                   bool        checkStackNeeded);

/**
 * Check that the argument is a valid pointer to a `Timeout` structure.  This
 * must have read/write permissions, be unsealed, and must not be a heap
 * address.
 */
bool __cheri_libcall check_timeout_pointer(const struct Timeout *timeout);

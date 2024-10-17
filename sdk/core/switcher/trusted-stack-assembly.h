// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <assembly-helpers.h>
EXPORT_ASSEMBLY_OFFSET(TrustedStack, mepcc, 0 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, cra, 1 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, csp, 2 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, cgp, 3 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ctp, 4 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ct0, 5 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ct1, 6 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ct2, 7 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, cs0, 8 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, cs1, 9 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ca0, 10 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ca1, 11 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ca2, 12 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ca3, 13 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ca4, 14 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, ca5, 15 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, hazardPointers, 16 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, mstatus, 17 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, mcause, (17 * 8) + 4)
#ifdef CONFIG_MSHWM
EXPORT_ASSEMBLY_OFFSET(TrustedStack, mshwm, 18 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, mshwmb, (18 * 8) + 4)

// Size of everything up to this point
#	define TSTACK_REGFRAME_SZ (19 * 8)
// frameoffset and padding
#	define TSTACK_HEADER_SZ 16
#else
// Size of everything up to this point
#	define TSTACK_REGFRAME_SZ ((17 * 8) + (2 * 4))
// frameoffset and padding
#	define TSTACK_HEADER_SZ 8
#endif
// The basic trusted stack is the size of the save area, 8 bytes of state for
// unwinding information, and then a single trusted stack frame used for the
// unwind state of the initial thread. (8 * 3) is the size of TrustedStackFrame
// and will match the value below.
EXPORT_ASSEMBLY_SIZE(TrustedStack,
                     TSTACK_REGFRAME_SZ + TSTACK_HEADER_SZ + (8 * 3))
EXPORT_ASSEMBLY_OFFSET(TrustedStack,
                       frames,
                       TSTACK_REGFRAME_SZ + TSTACK_HEADER_SZ)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, frameoffset, TSTACK_REGFRAME_SZ)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, threadID, TSTACK_REGFRAME_SZ + 2)

EXPORT_ASSEMBLY_OFFSET(TrustedStackFrame, csp, 0)
EXPORT_ASSEMBLY_OFFSET(TrustedStackFrame, calleeExportTable, 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStackFrame, errorHandlerCount, 16)
// If you change this value, you must replace size_to_trusted_stack_frames in
// entry.S with something that divides by the new size.
EXPORT_ASSEMBLY_SIZE(TrustedStackFrame, (8 * 3))

#define TSTACKOFFSET_FIRSTFRAME                                                \
	(TrustedStack_offset_frameoffset + TSTACK_HEADER_SZ)

/* Constant to represent the raw permissions of the compartment CSP.
 *  We use this in the switcher, to verify the CSP comes from the
 *  compartment is exactly what we expect.
 *  This represents the following permissions:
 *  Load, Store, LoadStoreCapability, LoadMutable StoreLocal and LoadGlobal
 */
#define COMPARTMENT_STACK_PERMISSIONS 0x7e

/**
 * Space reserved at the top of a stack on entry to the compartment.
 *
 * This *must* be a multiple of 16, which is the stack alignment.
 */
#define STACK_ENTRY_RESERVED_SPACE 16

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
EXPORT_ASSEMBLY_OFFSET(TrustedStack, mshwm, 18 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, mshwmb, (18 * 8) + 4)

EXPORT_ASSEMBLY_OFFSET(TrustedStack, frameoffset, 19 * 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, threadID, (19 * 8) + 2)
EXPORT_ASSEMBLY_OFFSET(TrustedStack, frames, 20 * 8)

// The basic trusted stack is the size of the save area, 8 bytes of state for
// unwinding information, and then a single trusted stack frame used for the
// unwind state of the initial thread. (3 * 8) is the size of TrustedStackFrame
// and will match the value below
EXPORT_ASSEMBLY_SIZE(TrustedStack, (20 * 8) + (3 * 8))

EXPORT_ASSEMBLY_OFFSET(TrustedStackFrame, csp, 0)
EXPORT_ASSEMBLY_OFFSET(TrustedStackFrame, calleeExportTable, 8)
EXPORT_ASSEMBLY_OFFSET(TrustedStackFrame, cpuFeatures, 16)
EXPORT_ASSEMBLY_OFFSET(TrustedStackFrame, errorHandlerCount, 18)
EXPORT_ASSEMBLY_SIZE(TrustedStackFrame, (3 * 8))

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

/**
 * @file
 *
 * Ibex defines some custom M-mode CSRs to control some of its extensions:
 *
 * - cpuctrlsts: "CPU Control and Status Register"
 * - secureseed: "Security Feature Seed Register"
 *
 * See https://ibex-core.readthedocs.io/en/latest/03_reference/cs_registers.html
 *
 * The convention for these names is that they begin with `IbexCSR_` followed
 * by the upstream name of the CSR.  `_index` then gives that CSR's index.  Each
 * field within has two preprocessor macros associated: one, named by the field
 * without suffix, provides a numeric value with a span of asserted bits
 * covering all associated bits within the 32-bit CSR word, and the other, named
 * with a `_shift` suffix, gives the 0-based bit index of the least significant
 * bit of the field's span.
 */

/*
 * The symbols herein are all integer literals and named not as the C
 * preprocessor macros they are but rather as if they were constants defined by
 * assembly-helpers.h's EXPORT_ASSEMBLY_... macros.
 *
 * NOLINTBEGIN(readability-identifier-naming)
 */

/**
 * Index of the cpuctrlsts CSR.
 *
 * See
 * https://ibex-core.readthedocs.io/en/latest/03_reference/cs_registers.html#cpu-control-and-status-register-cpuctrlsts
 */
#define IbexCSR_Cpuctrlsts_index 0x7C0U

/// Instruction cache enable, active high
#define IbexCSR_Cpuctrlsts_IcacheEnable 0x001U
#define IbexCSR_Cpuctrlsts_IcacheEnable_shift 0

/// Data independent timing features enabled, active high
#define IbexCSR_Cpuctrlsts_DataIndependentTiming 0x002U
#define IbexCSR_Cpuctrlsts_DataIndependentTiming_shift 1

/**
 * Dummy instruction insertion enabled, active high.
 *
 * See
 * https://ibex-core.readthedocs.io/en/latest/03_reference/security.html#security
 */
#define IbexCSR_Cpuctrlsts_DummyInstructionInsertion 0x004U
#define IbexCSR_Cpuctrlsts_DummyInstructionInsertion_shift 2

/**
 * Dummy instruction insertion rate.
 *
 * See
 * https://ibex-core.readthedocs.io/en/latest/03_reference/security.html#security
 */
#define IbexCSR_Cpuctrlsts_DummyInstructionRate 0x038U
#define IbexCSR_Cpuctrlsts_DummyInstructionRate_shift 3

/// Synchronous exception taken and core has not yet executed `mret`
#define IbexCSR_Cpuctrlsts_SynchronousExceptionSeen 0x040U
#define IbexCSR_Cpuctrlsts_SynchronousExceptionSeen_shift 6

/// Synchronous exception taken while SYNC_EXC_SEEN was asserted
#define IbexCSR_Cpuctrlsts_DoubleFaultSeen 0x080U
#define IbexCSR_Cpuctrlsts_DoubleFaultSeen_shift 7

/// Instruction cache scrambling key is valid
#define IbexCSR_Cpuctrlsts_IcacheScrambleKeyValid 0x100U
#define IbexCSR_Cpuctrlsts_IcacheScrambleKeyValid_shift 8

/**
 * Index of the secureseed CSR.
 *
 * See
 * https://ibex-core.readthedocs.io/en/latest/03_reference/cs_registers.html#cpu-control-and-status-register-secureseed
 *
 * This CSR holds a single, 32-bit, numeric value.
 */
#define IbexCSR_Secureseed_index 0x7C1U

// NOLINTEND(readability-identifier-naming)

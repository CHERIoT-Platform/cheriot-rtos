// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "platform-switcher_cpu_features.hh"

/// A mask of all bits that can be set in cpuFeatures
#	define SWITCHER_CPU_FEATURE_PLATFORM_MASK                                 \
		(IbexCSR_Cpuctrlsts_IcacheEnable |                                     \
		 IbexCSR_Cpuctrlsts_DataIndependentTiming)

// clang-format off

.macro platform_switcher_cpu_features_activate newValue, scratch
	// Mask out bits not available for user control
	andi               \newValue, \newValue, SWITCHER_CPU_FEATURE_PLATFORM_MASK

	// OR those in with the other bits in the register and write
	csrr               \scratch, IbexCSR_Cpuctrlsts_index
	andi               \scratch, \scratch, ~SWITCHER_CPU_FEATURE_PLATFORM_MASK
	or                 \scratch, \scratch, \newValue
	csrw               IbexCSR_Cpuctrlsts_index, \scratch

	// Read back the bits available for user control
	csrr               \newValue, IbexCSR_Cpuctrlsts_index
	andi               \newValue, \newValue, SWITCHER_CPU_FEATURE_PLATFORM_MASK
.endm

.macro platform_switcher_cpu_features_reactivate newValue, scratch
	// newValue is known to be masked, so just RMW the CSR
	csrr               \scratch, IbexCSR_Cpuctrlsts_index
	andi               \scratch, \scratch, ~SWITCHER_CPU_FEATURE_PLATFORM_MASK
	or                 \scratch, \scratch, \newValue
	csrw               IbexCSR_Cpuctrlsts_index, \scratch
.endm

// clang-format on

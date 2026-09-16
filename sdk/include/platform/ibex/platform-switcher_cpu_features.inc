// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "platform-switcher_cpu_features.hh"

/// A mask of all bits that can be set in cpuFeatures
#	define SWITCHER_CPU_FEATURE_PLATFORM_MASK                                 \
		(IbexCSR_Cpuctrlsts_IcacheEnable |                                     \
		 IbexCSR_Cpuctrlsts_DataIndependentTiming)

// clang-format off

.macro platform_switcher_cpu_features_mask newValue, scratch
	andi               \newValue, \newValue, SWITCHER_CPU_FEATURE_PLATFORM_MASK
.endm

.macro platform_switcher_cpu_features_activate newValue, scratch
	csrr               \scratch, IbexCSR_Cpuctrlsts_index
	andi               \scratch, \scratch, SWITCHER_CPU_FEATURE_PLATFORM_MASK
	or                 \scratch, \scratch, \newValue
	csrw               IbexCSR_Cpuctrlsts_index, \scratch
.endm

.macro platform_switcher_cpu_features_load_activate newValue, scratch, ty, mem
	cl\ty              \newValue, \mem
	platform_switcher_cpu_features_activate \newValue, \scratch
.endm

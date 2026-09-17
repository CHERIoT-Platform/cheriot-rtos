// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "ibex-csr.h"

/**
 * Flag to pass to switcher_invocation_cpu_features_set to enable or disable the
 * Ibex I$.
 */
#define SWITCHER_CPU_FEATURE_CACHE_INSTRUCTION_ENABLED                         \
	IbexCSR_Cpuctrlsts_IcacheEnable

/**
 * Flag to pass to switcher_invocation_cpu_features_set to enable or disable
 * Ibex's "data independent timing" behaviors.
 */
#define SWITCHER_CPU_FEATURE_PLATFORM_DATA_INDEPENDENT_TIMING                  \
	IbexCSR_Cpuctrlsts_DataIndependentTiming

/// Default to I$ on and "data independent timing" off.
#define SWITCHER_CPU_FEATURE_DEFAULT IbexCSR_Cpuctrlsts_IcacheEnable

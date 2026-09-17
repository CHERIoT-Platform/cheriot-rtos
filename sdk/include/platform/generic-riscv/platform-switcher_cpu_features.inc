// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "platform-switcher_cpu_features.hh"

// No-op macros for the switcher

.macro platform_switcher_cpu_features_mask newValue, scratch
.endm

.macro platform_switcher_cpu_features_activate newValue, scratch
.endm

.macro platform_switcher_cpu_features_load_activate scratch1, scratch2, ty, mem
.endm

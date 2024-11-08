// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <assembly-helpers.h>

EXPORT_ASSEMBLY_OFFSET(__jmp_buf, __cs0, 0)
EXPORT_ASSEMBLY_OFFSET(__jmp_buf, __cs1, 8)
EXPORT_ASSEMBLY_OFFSET(__jmp_buf, __csp, 16)
EXPORT_ASSEMBLY_OFFSET(__jmp_buf, __cra, 24)

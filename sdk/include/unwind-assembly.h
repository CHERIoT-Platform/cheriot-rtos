// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <assembly-helpers.h>
#include <setjmp-assembly.h>

#define INVOCATION_LOCAL_UNWIND_LIST_OFFSET 8

EXPORT_ASSEMBLY_OFFSET(CleanupList, next, 0)
EXPORT_ASSEMBLY_OFFSET(CleanupList, env, 8)

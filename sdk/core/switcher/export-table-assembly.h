// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <assembly-helpers.h>

EXPORT_ASSEMBLY_OFFSET(ExportEntry, functionStart, 0)
EXPORT_ASSEMBLY_OFFSET(ExportEntry, minimumStackSize, 2)
EXPORT_ASSEMBLY_OFFSET(ExportEntry, flags, 3)
EXPORT_ASSEMBLY_OFFSET(ExportTable, pcc, 0)
EXPORT_ASSEMBLY_OFFSET(ExportTable, cgp, 8)
EXPORT_ASSEMBLY_OFFSET(ExportTable, errorHandler, 16)

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>

/**
 * Basic memory safety bug classes
 */
enum class MemorySafetyBugClass
{
	StackLinearOverflow,
	HeapLinearOverflow,
	HeapNonlinearOverflow,
	HeapUseAfterFree,
	StoreStackPointerToGlobal
};

int __cheri_compartment("memory_safety_inner")
  memory_safety_inner_entry(MemorySafetyBugClass operation);
// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <compartment.h>
#include <debug.hh>
#include <stdlib.h>

using Debug = ConditionalDebug<DEBUG_ALLOCATOR, "Allocator">;

#define ABORT()                                                                \
	Debug::Invariant(false, "Unrecoverable allocator corruption detected")

#ifdef NDEBUG
#	define RTCHECK(e) (1)
#else
#	define RTCHECK(e) (e)
constexpr size_t MStateSanityInterval = 128;
#endif

constexpr size_t MallocAlignShift = 3;

constexpr size_t MallocAlignment = 1U << MallocAlignShift;
constexpr size_t MallocAlignMask = MallocAlignment - 1;

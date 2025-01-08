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

constexpr StackCheckMode StackMode =
#if CHERIOT_STACK_CHECKS_ALLOCATOR
  StackCheckMode::Asserting
#else
  StackCheckMode::Disabled
// Uncomment if checks failed to find the correct values
// StackCheckMode::Logging
#endif
  ;

#if defined(__CHERIOT__) && (__CHERIOT__ >= 20250108)
#	define STACK_CHECK(expected)                                              \
		static_assert((expected) == __cheriot_minimum_stack__,                 \
		              "Explicit stack check does not match annotation!");      \
		StackUsageCheck<StackMode,                                             \
		                __cheriot_minimum_stack__,                             \
		                __PRETTY_FUNCTION__>                                   \
		  stackCheck
#else
#	define STACK_CHECK(expected)                                              \
		StackUsageCheck<StackMode, expected, __PRETTY_FUNCTION__> stackCheck
#endif

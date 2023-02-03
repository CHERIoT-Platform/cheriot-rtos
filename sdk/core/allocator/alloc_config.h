// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <debug.hh>
#include <stdlib.h>
#include <compartment.h>

using Debug = ConditionalDebug<DEBUG_ALLOCATOR, "Allocator">;

#define ABORT()                                                                \
	Debug::Invariant(false, "Unrecoverable allocator corruption detected")

#ifdef NDEBUG
#	define RTCHECK(e) (1)
#else
#	define RTCHECK(e) (e)
constexpr size_t MSTATE_SANITY_INTERVAL = 128;
#endif

constexpr size_t MALLOC_ALIGNSHIFT = 3;
constexpr size_t MALLOC_ALIGNMENT  = 1U << MALLOC_ALIGNSHIFT;
constexpr size_t MALLOC_ALIGNMASK  = MALLOC_ALIGNMENT - 1;

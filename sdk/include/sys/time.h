// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
#pragma once
/**
 * \file
 * POSIX time definitions.  This is *not* a complete implementation of the POSIX
 * spec.  The following are not implemented in CHERIoT RTOS:
 *
 *  -  `FD_CLR()`, `FD_ISSET()`, `FD_SET(), `FD_ZERO()`, `FD_SETSIZE`, and
 * `fd_set`, used for `select`.
 *  - `select` is not implemented because we do not have the POSIX file
 * descriptor abstraction, use the APIs in `multiwaiter.h` to wait for multiple
 * event sources.
 *  - `utimes`, which depends on a filesystem.
 *  - `getitimer` and `setitimer`, which are deprecated in POSIX.
 */

#include <cdefs.h>
#include <stdint.h>

// The names in this file come from C or POSIX and so do not correspond to our
// naming scheme.  This header is expected to be included in C, so should also
// not provide warnings about void in function parameter lists.
// NOLINTBEGIN(readability-identifier-naming,modernize-redundant-void-arg)

/// Type for holding seconds
typedef int64_t time_t;
/// Type for holding microseconds up to one second, which requires 20 bits.
typedef uint32_t suseconds_t;

/**
 * Type used for time values as seconds and microseconds.
 */
struct timeval
{
	/// Number of seconds.
	time_t tv_sec;
	/// Number of microseconds (must be less than 1,000,000)
	suseconds_t tv_usec;
};

/**
 * Get the current wall-clock time.  The time-zone parameter is unused.
 */
__cheriot_libcall int gettimeofday(struct timeval *__restrict tp,
                                   void *__restrict tzp);

// NOLINTEND(readability-identifier-naming,modernize-redundant-void-arg)

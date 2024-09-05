#pragma once
// SPDX-License-Identifier: MIT
// Copyright CHERIoT Contributors

/**
 * This header is part of C11 (and supported for compatibility in older
 * versions) but is gone in C23 because the C keywords matching their C++
 * equivalents were added.
 */
#ifdef __STDC_VERSION__
#	if __STDC_VERSION__ < 202311L

/**
 * C++-compatible spelling for `_Alignas`.
 */
#		define alignas(__x) _Alignas(__x)

/**
 * C++-compatible spelling for `_Alignof`.
 */
#		define alignof(__x) _Alignof(__x)

#		define __alignas_is_defined 1
#		define __alignof_is_defined 1

#	endif
#endif

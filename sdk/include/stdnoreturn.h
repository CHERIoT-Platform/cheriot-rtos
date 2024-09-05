#pragma once
// SPDX-License-Identifier: MIT
// Copyright CHERIoT Contributors

/**
 * This header is part of C11 (and supported for compatibility in older
 * versions) but is gone in C23 because the C keywords were moved out of the
 * reserved-for-the-implementation namespace into the global one.
 */
#ifdef __STDC_VERSION__
#	if __STDC_VERSION__ < 202311L

/**
 * C++-compatible spelling for `_Noreturn`.
 */
#		define noreturn _Noreturn

#	endif
#endif

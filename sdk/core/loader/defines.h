// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#define BOOT_STACK_SIZE 1024
/**
 * The trusted stack size for the loader.
 * This is used for calling initialisers and so must be one deep.  The switcher
 * expects the parent frame to always exist, so it must actually be two deep.
 */
#define BOOT_TSTACK_SIZE                                                       \
	(TrustedStack_offset_frames + (2 * TrustedStackFrame_size))

#define BOOT_THREADINFO_SZ 16

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#define BOOT_STACK_SIZE 1024
/**
 * The trusted stack size for the loader. Since the loader, the scheduler and
 * the idle thread must not do compartment calls, this trusted stack only needs
 * to have a register frame and not trusted stack frames.
 */
#define BOOT_TSTACK_SIZE TrustedStack_offset_frames

// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <compartment-macros.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Checks that `ptr` is valid, unsealed, has at least `rawPermissions`, and has
 * at least `space` bytes after the current offset.
 *
 * If the permissions do not include Global and `checkStackNeeded` is `false`,
 * then this will also check that the capability does not point to the current
 * thread's stack.
 *
 * To reduce code size, this function is provided as part of the compartment
 * helper library.
 */
bool __cheri_libcall check_pointer(const void *ptr,
                                   size_t      space,
                                   uint32_t    rawPermissions,
                                   bool        checkStackNeeded);

// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <assembly-helpers.h>

/*
 * Constant to represent the raw permissions of the compartment CSP.  We use
 * this in the switcher, to verify the permissions of the CSP that comes from
 * the compartment are exactly what we expect.
 */
EXPORT_ASSEMBLY_EXPRESSION(COMPARTMENT_STACK_PERMISSIONS,
                           (CHERI::PermissionSet{
                             CHERI::Permission::Load,
                             CHERI::Permission::Store,
                             CHERI::Permission::LoadStoreCapability,
                             CHERI::Permission::LoadMutable,
                             CHERI::Permission::StoreLocal,
                             CHERI::Permission::LoadGlobal}
                              .as_raw()),
                           0x7e)

/**
 * Space reserved at the top of a stack on entry to the compartment.
 *
 * This *must* be a multiple of 16, which is the stack alignment.
 */
#define STACK_ENTRY_RESERVED_SPACE 16

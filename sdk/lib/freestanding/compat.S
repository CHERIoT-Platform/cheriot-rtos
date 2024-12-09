// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * This file contains compatibility aliases for exporting the freestanding
 * library functions as both mangled and unmangled symbols.
 */

/**
 * Given a function named `function_name`, export it as a library function
 * named `export_function_name`.
 */
.macro EXPORT_COMPATIBILITY_ALIAS export_function_name, function_name, flags
	.section .compartment_exports,"aR",@progbits
	.type    __library_export_libcalls_\export_function_name\(),@object
	.global  __library_export_libcalls_\export_function_name\()
    .p2align 2
  __library_export_libcalls_\export_function_name\():
	.half \function_name - __compartment_pcc_start
	// Stack usage: Ignored for library exports
	.byte 0
	// Flags, only interrupt state is used for library exports, 0 is inherited
	.byte \flags
	.size __library_export_libcalls_\export_function_name, 40
	.previous
.endm

EXPORT_COMPATIBILITY_ALIAS _Z6memcmpPKvS0_j, memcmp, 3
EXPORT_COMPATIBILITY_ALIAS _Z6memcpyPvPKvj, memcpy, 3
EXPORT_COMPATIBILITY_ALIAS _Z6memsetPvij, memset, 2
EXPORT_COMPATIBILITY_ALIAS _Z7memmovePvPKvj, memmove, 3

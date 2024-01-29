#pragma once

/**
 * Manually build an export table with the given prefix.  Internal use only;
 * please use one of the macros below instead.
 */
.macro CHERIOT_EXPORT_FUNCTION_INTERNAL prefix, function_name, \
  minimum_stack, flags

	.section .compartment_exports,"awR",@progbits
	.type    \prefix\()_\function_name\(),@object
	.global  \prefix\()_\function_name\()
    .p2align 2
  \prefix\()_\function_name\():
	.half \function_name - __compartment_pcc_start
	.byte \minimum_stack
	.byte \flags
	.size \prefix\()_\function_name, 4
	.previous
.endm

/**
 * Manually build an export table entry in the named compartment.
 */
.macro CHERIOT_COMPARTMENT_EXPORT_FUNCTION compartment_name, function_name, \
  minimum_stack, flags
	CHERIOT_EXPORT_FUNCTION_INTERNAL __export_\compartment_name\(), \
	  \function_name, \minimum_stack, \flags
.endm

/**
 * Manually build an export table entry in the current compartment.
 */
.macro CHERIOT_EXPORT_FUNCTION function_name, minimum_stack, flags
	CHERIOT_COMPARTMENT_EXPORT_FUNCTION \
	  __CHERI_COMPARTMENT__, \function_name, \minimum_stack, \flags
.endm

/**
 * Manually build a libcall export table entry
 */
.macro CHERIOT_EXPORT_LIBCALL function_name, minimum_stack, flags
	CHERIOT_EXPORT_FUNCTION_INTERNAL __library_export_libcalls, \
	  \function_name, \minimum_stack, \flags
.endm

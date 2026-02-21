#include <unwind-assembly.h>

/**
 * A direct re-implementation of unwind.h's cleanup_unwind() as a stackless
 * error handler.
 *
 * If there is no registered CleanupList structure (equivalently, there's no
 * CHERIOT_DURING block active at the time of the fault), then this requests
 * unwinding out of the compartment.  Otherwise, we will longjmp() out to the
 * indicated handler (that is, the CHERIOT_HANDLER block associated with the
 * current CHERIOT_DURING block), having reset the compartment error handler
 * invocation counter to zero.
 */
.section .compartment_error_handler_stackless,"aw",@progbits
.globl compartment_error_handler_stackless
.p2align 2
.type compartment_error_handler_stackless,@function
compartment_error_handler_stackless:
// Get the head of the error list (see cleanup_list_head)
	cgettop      t0, csp
	csetaddr     csp, csp, t0
	clc          cs0, -INVOCATION_LOCAL_UNWIND_LIST_OFFSET(csp)
	beqz         s0, .Lforce_unwind
// Pop the top error from the list
	clc          ct0, CleanupList_offset_next(cs0)
	csc          ct0, -INVOCATION_LOCAL_UNWIND_LIST_OFFSET(csp)
// Mark this error handler as having finished.  We may still trap again
// and reenter this, but now that we've popped the top element from the
// stack we will run some different cleanup code next time. */
.Llookup_reset:
	auipcc          ct2, %cheriot_compartment_hi(__library_import_libcalls__Z39switcher_handler_invocation_count_resetv)
	cincoffset      x0, x0, x0 // Mandatory 4-byte nop
	clc             ct2, %cheriot_compartment_lo_i(.Llookup_reset)(ct2)
	cjalr           ct2
// longjmp to the error handler.
	clc          cs1, (CleanupList_offset_env + __jmp_buf_offset___cs1)(cs0)
	clc          csp, (CleanupList_offset_env + __jmp_buf_offset___csp)(cs0)
	clc          cra, (CleanupList_offset_env + __jmp_buf_offset___cra)(cs0)
	clc          cs0, (CleanupList_offset_env + __jmp_buf_offset___cs0)(cs0)
	cjr          cra

.Lforce_unwind:
	li           a0, 1
	cret

	.section	.compartment_imports,"aG",@progbits,__library_import_libcalls__Z39switcher_handler_invocation_count_resetv,comdat
	.type	__library_import_libcalls__Z39switcher_handler_invocation_count_resetv,@object
	.weak	__library_import_libcalls__Z39switcher_handler_invocation_count_resetv
	.p2align	3
__library_import_libcalls__Z39switcher_handler_invocation_count_resetv:
	.word	__library_export_libcalls__Z39switcher_handler_invocation_count_resetv+1
	.word	0
	.size	__library_import_libcalls__Z39switcher_handler_invocation_count_resetv, 8


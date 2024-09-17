.section .compartment_error_handler_stackless,"aw",@progbits
.globl compartment_error_handler_stackless
.p2align 2
.type compartment_error_handler_stackless,@function
compartment_error_handler_stackless:
// Get the head of the error list.
	cgettop      t0, csp
	csetaddr     csp, csp, t0
	clc          cs0, -8(csp)
	beqz         s0, .Lforce_unwind
// Pop the top error from the list. */
	clc          ct0, 0(cs0)
	csc          ct0, -8(csp)
// Mark this error handler as having finished.  We may still trap again
// and reenter this, but now that we've popped the top element from the
// stack we will run some different cleanup code next time. */
.Llookup_reset:
	auipcc          ct2, %cheriot_compartment_hi(__library_import_libcalls__Z39switcher_handler_invocation_count_resetv)
	clc             ct2, %cheriot_compartment_lo_i(.Llookup_reset)(ct2)
	cjalr           ct2
// longjmp to the error handler.
	clc          cs1, 16(cs0)
	clc          csp, 24(cs0)
	clc          cra, 32(cs0)
	clc          cs0, 8(cs0)
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


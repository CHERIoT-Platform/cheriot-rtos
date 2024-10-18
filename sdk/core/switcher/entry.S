// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "export-table-assembly.h"
#include "trusted-stack-assembly.h"
#include <errno.h>

.include "assembly-helpers.s"

#  Symbolic names for the stack high water mark registers until
#  the assembler knows about them.

/**
 * Machine-mode stack high water mark CSR
 */
#define CSR_MSHWM  0xbc1
/**
 * Machine mode stack high water mark stack base CSR
 */
#define CSR_MSHWMB 0xbc2

#define MAX_FAULTS_PER_COMPARTMENT_CALL 1024

#define SPILL_SLOT_cs0 0
#define SPILL_SLOT_cs1 8
#define SPILL_SLOT_cgp 16
#define SPILL_SLOT_pcc 24
#define SPILL_SLOT_SIZE 32

/*
 * The switcher uniformly speaks of registers using their RISC-V ELF psABI names
 * and not their raw index, as, broadly speaking, we use registers in a similar
 * way to C functions.  However, it's probably convenient to have a mapping
 * readily accessible, so here 'tis:
 *
 *  #     x0   x1 x2 x3 x4 x5 x6 x7 x8 x9 x10 x11 x12 x13 x14 x15
 *  psABI zero ra sp gp tp t0 t1 t2 s0 s1 a0  a1  a2  a3  a4  a5
 *
 * When we use the psABI name without a 'c' prefix, we are sometimes meaning to
 * refer to the address component of the capability.
 *
 * Despite the use of psABI names and conformance at the interface (argument
 * registers used for arguments, return address register used for its canonical
 * purpose, &c), one should not read too much of the psABI calling convention
 * into the code here.  Within the switcher, the machine is a raw register
 * machine and C is a distant, high-level language.
 */

switcher_code_start:

# Global for the sealing key.  Stored in the switcher's code section.
	.section .text, "ax", @progbits
	.globl compartment_switcher_sealing_key
	.p2align 3
compartment_switcher_sealing_key:
	.long 0
	.long 0
# Global for the scheduler's PCC.  Stored in the switcher's code section.
.section .text, "ax", @progbits
	.globl switcher_scheduler_entry_pcc
	.p2align 3
switcher_scheduler_entry_pcc:
	.long 0
	.long 0
# Global for the scheduler's CGP.  Stored in the switcher's code section.
.section .text, "ax", @progbits
	.globl switcher_scheduler_entry_cgp
	.p2align 3
switcher_scheduler_entry_cgp:
	.long 0
	.long 0
# Global for the scheduler's CSP.  Stored in the switcher's code section.
.section .text, "ax", @progbits
	.globl switcher_scheduler_entry_csp
	.p2align 2
switcher_scheduler_entry_csp:
	.long 0
	.long 0

/**
 * Copy a register context from `src` to `dst` using `scratch` as the register
 * to hold loaded capabilities and `counter` as the register to hold the loop
 * counter.  All four registers are clobbered by this macro.
 */
.macro copyContext dst, src, scratch, counter
	    addi           \counter, zero, 15
	1:
	    clc            \scratch, 0(\src)
	    csc            \scratch, 0(\dst)
	    addi           \counter, \counter, -1
	    cincoffset     \dst, \dst, 8
	    cincoffset     \src, \src, 8
	    bnez           \counter, 1b
.endm

/// Spill a single register to a trusted stack pointed to by csp.
.macro spillOne, reg
	csc \reg, TrustedStack_offset_\reg(csp)
.endm

/**
 * Spill all of the registers in the list (in order) to a trusted stack pointed
 * to by csp.
 */
.macro spillRegisters reg1, regs:vararg
	forall spillOne, \reg1, \regs
.endm

/// Reload a single register from a trusted stack pointed to by csp.
.macro reloadOne, reg
	clc \reg, TrustedStack_offset_\reg(csp)
.endm

/**
 * Reload all of the registers in the list (in order) to a trusted stack pointed
 * to by csp.
 */
.macro reloadRegisters reg1, regs:vararg
	forall reloadOne, \reg1, \regs
.endm

/**
 * Verify the compartment stack is valid, with the expected permissions and
 * unsealed.
 * This macro assumes t2 and tp are available to use.
 */
.macro check_compartment_stack_integrity reg
	// Check that the caller's CSP is a tagged, unsealed capability (with at
	// least load permission - we'll check the other permissions properly
	// later) by loading a byte.  If this doesn't work, we'll fall off this
	// path into the exception handler and force unwind.
	clb                t2, 0(\reg)
	// make sure the caller's CSP has the expected permissions
	cgetperm           t2, \reg
	li                 tp, COMPARTMENT_STACK_PERMISSIONS
	bne                tp, t2, .Lforce_unwind
	// Check that the top and base are 16-byte aligned
	cgetbase           t2, csp
	or                 t2, t2, sp
	andi               t2, t2, 0xf
	bnez               t2, .Lforce_unwind
.endm

/**
 * Zero the stack.  The three operands are the base address, the top address,
 * and a scratch register to use.  The base must be a capability but it must
 * be provided without the c prefix because it is used as both a capability
 * and integer register.  All three registers are clobbered.
 */
.macro zero_stack base top scratch
	addi               \scratch, \top, -32
	addi               \top, \top, -16
	bgt                \base, \scratch, 1f
	// Zero the stack in 32-byte chunks
0:
	csc                cnull, 0(c\base)
	csc                cnull, 8(c\base)
	csc                cnull, 16(c\base)
	csc                cnull, 24(c\base)
	cincoffset         c\base, c\base, 32
	ble                \base, \scratch, 0b
1:
	bgt                \base, \top, 2f
	// Zero any 16-byte tail
	csc                cnull, 0(c\base)
	csc                cnull, 8(c\base)
2:
.endm

/**
 * Clear the hazard pointers associated with this thread.  We don't care about
 * leaks here (they're store-only from anywhere except the allocator), so just
 * write a 32-bit zero over half of each one to clobber the tags.
 */
.macro clear_hazard_slots trustedStack, scratch
	clc                \scratch, TrustedStack_offset_hazardPointers(\trustedStack)
	csw                zero, 0(\scratch)
	csw                zero, 8(\scratch)
.endm

	.section .text, "ax", @progbits
	.globl __Z26compartment_switcher_entryz
	.p2align 2
	.type __Z26compartment_switcher_entryz,@function
__Z26compartment_switcher_entryz:
	/*
	 * Spill caller-save registers carefully.  If we find ourselves unable to do
	 * so, we'll return an error to the caller (via the exception path; see
	 * .Lhandle_error_in_switcher).  The error handling path assumes that the
	 * first spill is to the lowest address and guaranteed to trap if any would.
	 */
	cincoffset        ct2, csp, -SPILL_SLOT_SIZE
.Lswitcher_entry_first_spill:
	csc               cs0, SPILL_SLOT_cs0(ct2)
	csc               cs1, SPILL_SLOT_cs1(ct2)
	csc               cgp, SPILL_SLOT_cgp(ct2)
	csc               cra, SPILL_SLOT_pcc(ct2)
	cmove             csp, ct2
	// before we access any privileged state, we can verify the
	// compartment's csp is valid. If not, force unwind.
	// Note that this check is purely to protect the callee, not the switcher
	// itself.
	check_compartment_stack_integrity csp
	// The caller should back up all callee saved registers.
	// mtdc should always have an offset of 0.
	cspecialr          ct2, mtdc
#ifndef NDEBUG
	// XXX: This line is useless, only for mtdc to show up in debugging.
	cmove              ct2, ct2
#endif
	clear_hazard_slots ct2, ctp

	// make sure the trusted stack is still in bounds
	clhu               tp, TrustedStack_offset_frameoffset(ct2)
	cgetlen            t2, ct2
	bgeu               tp, t2, .Lout_of_trusted_stack
	// we are past the stacks checks. Reload ct2; tp is still as it was
	cspecialr          ct2, mtdc
	// ctp points to the current available trusted stack frame.
	cincoffset         ctp, ct2, tp
	csc                csp, TrustedStackFrame_offset_csp(ctp)
	// We have just entered this call, so no faults triggered during this call
	// yet.
	csh                zero, TrustedStackFrame_offset_errorHandlerCount(ctp)
	// For now, store a null export entry so that we don't ever try to pass
	// switcher state to an error handler.
	csc                cnull, TrustedStackFrame_offset_calleeExportTable(ctp)
	clhu               s1, TrustedStack_offset_frameoffset(ct2)
	addi               s1, s1, TrustedStackFrame_size
	// Update the frame offset.
	// Any fault before this point (wrong target cap, unaligned stack, etc.) is
	// seen as a fault in the caller. From this point after writing the new
	// tstack offset, any fault is seen as a callee fault.  With a null export
	// table entry on the trusted stack, a fault here will cause a forced
	// unwind until we set the correct one.
	csh                s1, TrustedStack_offset_frameoffset(ct2)
	// Chop off the stack.
	cgetaddr           s0, csp
	cgetbase           s1, csp
	csetaddr           csp, csp, s1
	sub                s1, s0, s1
	csetboundsexact    ct2, csp, s1
	csetaddr           csp, ct2, s0
#ifdef CONFIG_MSHWM
	// Read the stack high water mark (which is 16-byte aligned)
	csrr               gp, CSR_MSHWM
	// Skip zeroing if high water mark >= stack pointer
	bge                gp, sp, .Lafter_zero
	// Use stack high water mark as base address for zeroing.  If this faults
	// then it will trigger a force unwind.  This can happen only if the caller
	// is doing something bad.
	csetaddr           ct2, csp, gp
#endif
	zero_stack         t2, s0, gp
.Lafter_zero:
	// Reserve space for unwind state and so on.
	cincoffset 	       csp, csp, -STACK_ENTRY_RESERVED_SPACE
#ifdef CONFIG_MSHWM
	// store new stack top as stack high water mark
	csrw               CSR_MSHWM, sp
#endif
	// Fetch the sealing key
	LoadCapPCC         cs0, compartment_switcher_sealing_key
	li                 gp, 9
	csetaddr           cs0, cs0, gp
	// The target capability is in ct1. Unseal, check tag and load the entry point offset.
	cunseal            ct1, ct1, cs0
	// Load the entry point offset.  If cunseal failed then this will fault and
	// we will force unwind.
	clhu               s0, ExportEntry_offset_functionStart(ct1)
	// At this point, we know that the cunseal has succeeded (we didn't trap on
	// the load) and so it's safe to store the unsealed value of the export
	// table pointer.  Nothing between this point and transition to the callee
	// should fault.
	csc                ct1, TrustedStackFrame_offset_calleeExportTable(ctp)

	// Load the minimum stack size required by the callee.
	clbu               tp, ExportEntry_offset_minimumStackSize(ct1)
	// The stack size is in 8-byte units, so multiply by 8.
	slli               tp, tp, 3
	// Check that the stack is large enough for the callee.
	// At this point, we have already truncated the stack and so the length of
	// the stack is the length that the callee can use.
	cgetlen            t2, csp
	// Include the space we reserved for the unwind state.
	addi               t2, t2, -STACK_ENTRY_RESERVED_SPACE
	bgtu               tp, t2, .Lstack_too_small

	// Get the flags field into tp
	clbu               tp, ExportEntry_offset_flags(ct1)
	cgetbase           s1, ct1
	csetaddr           ct1, ct1, s1
	// Load the target CGP
	clc                cgp, ExportTable_offset_cgp(ct1)
	// Load the target PCC and point to the function.
	clc                cra, ExportTable_offset_pcc(ct1)
	cincoffset         cra, cra, s0
	// Get the number of registers to zero in t2
	andi               t2, tp, 0x7
	// Get the interrupt-disable bit in t1
	andi               t1, tp, 0x10
	// Zero any unused argument registers
	// The low 3 bits of the flags field contain the number of arguments to
	// pass.  We create a small sled that zeroes them and jump into the middle
	// of it at an offset defined by the number of registers that the export
	// entry told us to pass.
.Lload_zero_arguments_start:
	auipcc             cs0, %cheriot_compartment_hi(.Lzero_arguments_start)
	cincoffset         cs0, cs0, %cheriot_compartment_lo_i(.Lload_zero_arguments_start)
	// Change from the number of registers to pass into the number of 2-byte
	// instructions to skip.
	sll                t2, t2, 1
	// Offset the jump target by the number of registers that we should be
	// passing.
	cincoffset         cs0, cs0, t2
	// Jump into the sled.
	cjr                cs0
.Lzero_arguments_start:
	zeroRegisters      a0, a1, a2, a3, a4, a5, t0
	// Enable interrupts of the interrupt-disable bit is not set in flags
	bnez               t1, .Lskip_interrupt_disable
	csrsi              mstatus, 0x8
.Lskip_interrupt_disable:
	// Registers passed to the callee are:
	// cra (c1), csp (c2), and cgp (c3) are passed unconditionally.
	// ca0-ca5 (c10-c15) and ct0 (c5) are either passed as arguments or cleared
	// above.  This should add up to 10 registers, with the remaining 5 being
	// cleared now:
	zeroRegisters      tp, t1, t2, s0, s1
	cjalr              cra

.Lskip_compartment_call:
	// If we are doing a forced unwind of the trusted stack then we do almost
	// exactly the same as a normal unwind.  We will jump here from the
	// exception path.
	cjal               .Lpop_trusted_stack_frame
	cmove              cra, ca2
	// Zero all registers apart from RA, GP, SP and return args.
	// cra, csp and cgp needed for the compartment
	// cs0 saved and restored on trusted stack
	// cs1 saved and restored on trusted stack
	// ca0, used for first return value
	// ca1, used for second return value
	zeroAllRegistersExcept ra, sp, gp, s0, s1, a0, a1
.Ljust_return:
	cret

	// If the stack is too small, we don't do the call, but to avoid leaking
	// any other state we still go through the same return path as normal.  We
	// set the return registers to -ENOTENOUGHSTACK and 0, so users can see
	// that this is the failure reason.
.Lstack_too_small:
	li                 a0, -ENOTENOUGHSTACK
	li                 a1, 0
	j                  .Lskip_compartment_call
.size compartment_switcher_entry, . - compartment_switcher_entry

	// the entry point of all exceptions and interrupts
	// For now, the entire routine is run with interrupts disabled.
	.global  exception_entry_asm
	.p2align 2
exception_entry_asm:
	// We do not trust the interruptee's context. We cannot use its stack in any way.
	// The save reg frame we can use is fetched from the tStack.
	// In general, mtdc holds the trusted stack register.  We are here with
	// interrupts off and precious few registers available to us, so swap it
	// with the csp (we'll put it back, later).
	cspecialrw         csp, mtdc, csp
#ifndef NDEBUG
	// XXX: This move is useless, but just for debugging in the simulator.
	cmove              csp, csp
#endif

	// If we read out zero, we've reentered the exception and are about to
	// trap.  Make sure that we end up in an architectural trap loop: clobber
	// mtcc, so that trapping attempts to vector to an untagged PCC, thereby
	// causing another (i.e., a third) trap in spillRegisters, below.
	//
	// While that's a good start, it does not guarantee that we end up in a
	// trap loop: the reentry will probably have put something non-zero into
	// mtdc, so we wouldn't hit this, and wouldn't loop, when we take that
	// third trap.  (Exactly what we'd do instead is hard to say; we'd try
	// spilling registers to an attacker-controlled pointer, at the very
	// least.) Therefore, clobber mtcc (!) to ensure that the certainly
	// upcoming third trap puts us in an architectural trap loop.  This is
	// slightly preferable to clearing mtdc, which would also ensure that we
	// looped, because the architectural loop is tighter and involves no
	// program text, making it easier for microarchitecture to detect.
	bnez               sp, .Lexception_entry_still_alive
	cspecialw          mtcc, csp
.Lexception_entry_still_alive:

	// csp now points to the save reg frame that we can use.
	// The guest csp (c2) is now in mtdc. Will be spilled later, but we
	// spill all the other 14 registers now.
	spillRegisters     cra, cgp, ctp, ct0, ct1, ct2, cs0, cs1, ca0, ca1, ca2, ca3, ca4, ca5

	// If a thread has exited then it will set a fake value in the mcause so
	// that the scheduler knows not to try to resume it.
.Lthread_exit:
	// mtdc got swapped with the thread's csp, store it and clobber mtdc with
	// zero.  The trusted stack pointer is solely in csp, now; if we take
	// another trap before a new one is installed, or if the scheduler enables
	// interrupts and we take one, we'll pull this zero out of mtdc, above.
	zeroOne            t1
	cspecialrw         ct1, mtdc, ct1
	csc                ct1, TrustedStack_offset_csp(csp)

	// Store the rest of the special registers
	cspecialr          ct0, mepcc
	csc                ct0, TrustedStack_offset_mepcc(csp)
	csrr               t1, mstatus
	csw                t1, TrustedStack_offset_mstatus(csp)
#ifdef CONFIG_MSHWM
	csrr               t1, CSR_MSHWM
	csw                t1, TrustedStack_offset_mshwm(csp)
	csrr               t1, CSR_MSHWMB
	csw                t1, TrustedStack_offset_mshwmb(csp)
#endif
	csrr               t1, mcause
	csw                t1, TrustedStack_offset_mcause(csp)

	// If we hit one of the exception conditions that we should let
	// compartments handle then deliver it to the compartment.
	// CHERI exception code.
	li                 a0, 0x1c
	beq                a0, t1, .Lhandle_error
	// Misaligned instruction, instruction access, illegal instruction,
	// breakpoint, misaligned load, load fault, misaligned store, and store
	// access faults are in the range 0-7
	li                 a0, 0x8
	bltu               t1, a0, .Lhandle_error

	// TODO: On an ecall, we don't need to save any caller-save registers

	// At this point, thread state is completely saved. Now prepare the
	// scheduler context.
	// Function signature of the scheduler entry point:
	// TrustedStack *exception_entry(TrustedStack *sealedTStack,
	//     size_t mcause, size_t mepc, size_t mtval)

	LoadCapPCC         ca5, compartment_switcher_sealing_key
	li                 gp, 10
	csetaddr           ca5, ca5, gp
	cseal              ca0, csp, ca5 // sealed trusted stack
	mv                 a1, t1 // mcause
	cgetaddr           a2, ct0 // mepcc address
	csrr               a3, mtval
	// Fetch the stack, cgp and the trusted stack for the scheduler.
	LoadCapPCC         csp, switcher_scheduler_entry_csp
	LoadCapPCC         cgp, switcher_scheduler_entry_cgp
	LoadCapPCC         cra, switcher_scheduler_entry_pcc

	// Zero everything apart from things explicitly passed to scheduler.
	// cra, csp and cgp needed for the scheduler compartment
	// ca0, used for the sealed trusted stack argument
	// ca1, used for mcause
	// ca2, used for mepc
	// ca3, used for mtval
	zeroAllRegistersExcept ra, sp, gp, a0, a1, a2, a3

	// Call the scheduler.  This returns the new thread in ca0.
	cjalr              cra
	// The scheduler may change interrupt posture or may trap, but if it
	// returns to us (that is, we reach here), the use of the sentry created by
	// cjalr will have restored us to deferring interrupts, and we will remain
	// in that posture until the mret in install_context.

	// Switch onto the new thread's trusted stack
	LoadCapPCC         ct0, compartment_switcher_sealing_key
	li                 gp, 10
	csetaddr           ct0, ct0, gp
	cunseal            csp, ca0, ct0
	clw                t0, TrustedStack_offset_mcause(csp)

	// Only now that we have done something that actually requires the tag of
	// csp be set, put it into mtdc.  If the scheduler has returned something
	// untagged or something with the wrong otype, the cunseal will have left
	// csp untagged and clw will trap with mtdc still 0.  If we made it here,
	// though, csp is tagged and so was tagged and correctly typed, and so it
	// is safe to install it to mtdc.  We won't cause traps between here and
	// mret, so reentrancy is no longer a concern.
	cspecialw          mtdc, csp

	// If mcause is 25, then we will jump into the error handler: another
	// thread has signalled that this thread should be interrupted.  25 is a
	// reserved exception number that we repurpose to indicate explicit
	// interruption.
	li                 t1, 25
	beq                t0, t1, .Lhandle_injected_error

	// Environment call from M-mode is exception code 11.
	// We need to skip the ecall instruction to avoid an infinite loop.
	li                 t1, 11
	clc                ct2, TrustedStack_offset_mepcc(csp)
	bne                t0, t1, .Linstall_context
	cincoffset         ct2, ct2, 4
	// Fall through to install context

// Install context expects csp and mtdc to point to the trusted stack and for
// ct2 to be the pcc to jump to.  All other registers are in unspecified states
// and will be overwritten when we install the context.
.Linstall_context:
	clw                ra, TrustedStack_offset_mstatus(csp)
	csrw               mstatus, ra
#ifdef CONFIG_MSHWM
	clw                ra, TrustedStack_offset_mshwm(csp)
	csrw               CSR_MSHWM, ra
	clw                ra, TrustedStack_offset_mshwmb(csp)
	csrw               CSR_MSHWMB, ra
#endif
	cspecialw          mepcc, ct2
	// csp (c2) will be loaded last and will overwrite the trusted stack pointer
	// with the thread's stack pointer.
	reloadRegisters cra, cgp, ctp, ct0, ct1, ct2, cs0, cs1, ca0, ca1, ca2, ca3, ca4, ca5, csp
	mret

// We are starting a forced unwind.  This is reached either when we are unable
// to run an error handler, or when we do run an error handler and it instructs
// us to return.  This treats all register values as undefined on entry.
.Lforce_unwind:
	// Pop the trusted stack frame.
	cjal               .Lpop_trusted_stack_frame
	cmove              cra, ca2
	// Zero all registers apart from RA, GP, SP and return args.
	// cra, cs0, cs1, and cgp were restored from the compartment's stack
	// csp restored from the trusted stack.
	// ca0, used for first return value
	// ca1, used for second return value
	zeroAllRegistersExcept ra, sp, gp, s0, s1, a0, a1
	li                 a0, -ECOMPARTMENTFAIL
	li                 a1, 0
	cret


// If we have run out of trusted stack, then just restore the caller's state
// and return an error value.
.Lout_of_trusted_stack:
	// Restore the spilled values
	clc                cs0, SPILL_SLOT_cs0(csp)
	clc                cs1, SPILL_SLOT_cs1(csp)
	clc                cra, SPILL_SLOT_pcc(csp)
	clc                cgp, SPILL_SLOT_cgp(csp)
	cincoffset         csp, csp, SPILL_SLOT_SIZE
	// Set the return registers
	li                 a0, -ENOTENOUGHTRUSTEDSTACK
	li                 a1, 0
	// Zero everything else
	zeroAllRegistersExcept ra, sp, gp, s0, s1, a0, a1
	cret

// If we have a possibly recoverable error, see if we have a useful error
// handler.  At this point, the register state will have been saved in the
// register-save area and so we just need to set up the environment.
// 
// On entry to this block, csp contains the trusted stack pointer, all other
// registers are undefined.
// 
// The handler will have this type signature:
// enum ErrorRecoveryBehaviour compartment_error_handler(struct ErrorState *frame,
//                                                       size_t             mcause,
//                                                       size_t             mtval);
.Lhandle_error:
	// We're now out of the exception path, so make sure that mtdc contains
	// the trusted stack pointer.
	cspecialw   mtdc, csp
	// Store an error value in return registers, which will be passed to the
	// caller on unwind.  They are currently undefined, if we leave this path
	// for a forced unwind then we will return whatever is in ca0 and ca1 to
	// the caller so must ensure that we don't leak anything.
	li                 a0, -1
	li                 a1, 0

	// We want to make sure we can't leak any switcher state into error
	// handlers, so if we're faulting in the switcher then we should force
	// unwind.  We never change the base of PCC in the switcher, so we can
	// check for this case by ensuring that the spilled mepcc and our current
	// pcc have the same base.
	auipcc             ct0, 0
	clc                ct1, TrustedStack_offset_mepcc(csp)
	cgetbase           t0, ct0
	cgetbase           tp, ct1
	beq                t0, tp, .Lhandle_error_in_switcher

	// Load the interrupted thread's stack pointer into ct0
	clc                ct0, TrustedStack_offset_csp(csp)
	// See if we can find a handler:
	clhu               tp, TrustedStack_offset_frameoffset(csp)
	li                 t1, TrustedStack_offset_frames
	beq                tp, t1, .Lreset_mepcc_and_install_context
	addi               tp, tp, -TrustedStackFrame_size

	// ctp points to the current available trusted stack frame.
	cincoffset         ctp, csp, tp
	// a0 indicates whether we're calling a stackless error handler (0: stack,
	// 1: stackless)
	li                 a0, 0

	// Allocate space for the register save frame on the stack.
	cincoffset         ct0, ct0, -(16*8)

	// WARNING: ENCODING SPECIFIC.
	// The following depends on the fact that before-the-start values are not
	// representable in the CHERIoT encoding and so will clear the tag.  If
	// this property changes then this will need to be replaced by a check that
	// against the base of the stack.  Note that this check can't be a simple
	// cgetbase on ct0, because moving the address below the base sufficiently
	// far that it's out of *representable* bounds will move the reported base
	// value (base is a displacement from the address).
	cgettag            t1, ct0
	// If there isn't enough space on the stack, see if there's a stackless
	// handler.
	beqz               t1, .Ltry_stackless_handler

	clc                ct1, TrustedStackFrame_offset_calleeExportTable(ctp)
	// Set the export table pointer to point to the *start* of the export
	// table.  It will currently point to the entry point that was raised.
	// TODO: We might want to pass this to the error handler, it might be
	// useful for providing per-entry-point error results.
	cgetbase           s0, ct1
	csetaddr           ct1, ct1, s0
	clhu               s0, ExportTable_offset_errorHandler(ct1)
	// A value of 0xffff indicates no error handler
	// If we found one, use it, otherwise fall through and try to find a
	// stackless handler.
	li                 s1, 0xffff
	bne                s0, s1, .Lhandler_found

.Ltry_stackless_handler:
	clc                ct1, TrustedStackFrame_offset_calleeExportTable(ctp)
	// Set the export table pointer to point to the *start* of the export
	// table.  It will currently point to the entry point that was raised.
	cgetbase           s0, ct1
	csetaddr           ct1, ct1, s0
	clhu               s0, ExportTable_offset_errorHandlerStackless(ct1)
	// A value of 0xffff indicates no error handler
	// Give up if there is no error handler for this compartment.
	li                 s1, 0xffff
	beq                s0, s1, .Lforce_unwind

	// The stack may have had its tag cleared at this point, so for stackless
	// handlers we need to restore the on-entry stack.
	// Get the previous trusted stack frame

	// Load the caller's csp
	clc                ct0, TrustedStackFrame_offset_csp(ctp)

	// If this is the top stack frame, then the csp field is the value on
	// entry.  If it's any other frame then we need to go to the previous one
	cincoffset         cs1, csp, TrustedStack_offset_frames
	beq                s1, tp, .Lrecovered_stack

	// The address of the stack pointer will point to the bottom of the
	// caller's save area, so we set the bounds to be the base up to the
	// current address.
	cgetaddr           a1, ct0
	cgetbase           a2, ct0
	sub                a1, a1, a2
	csetaddr           ct0, ct0, a2
	// The code that installs the context expects csp to be in ct0
	csetboundsexact    ct0, ct0, a1
.Lrecovered_stack:
	li                 a0, 1

.Lhandler_found:

	// Increment the handler invocation count.
	clhu               s1, TrustedStackFrame_offset_errorHandlerCount(ctp)
	addi               s1, s1, 1
	csh                s1, TrustedStackFrame_offset_errorHandlerCount(ctp)

	// If we are in a double fault, unwind now.  The low bit should be 1 while
	// we are handling a fault.
	andi               ra, s1, 1
	beqz               ra, .Lforce_unwind
	// If we have reached some arbitrary limit on the number of faults in a
	// singe compartment calls, give up now.
	// TODO: Make this a number based on something sensible, possibly something
	// set per entry point.  Some compartments (especially top-level ones)
	// should be allowed to fault an unbounded number of times.
	li                 ra, MAX_FAULTS_PER_COMPARTMENT_CALL
	bgtu               s1, ra, .Lforce_unwind

	// Load the pristine pcc and cgp for the invoked compartment.
	clc                cra, ExportTable_offset_pcc(ct1)
	clc                cgp, ExportTable_offset_cgp(ct1)
	// Set the jump target to the error handler entry point
	// This may result in something out-of-bounds if the compartment has a
	// malicious value for their error handler (hopefully caught at link or
	// load time), but if it does then we will double-fault and force unwind.
	cgetbase           s1, cra
	csetaddr           cra, cra, s1
	cincoffset         cra, cra, s0

	// If we're in an error handler with a stack, set up the stack, otherwise
	// we just need to set up argument registers.
	beqz               a0, .Lset_up_stack_handler
	clw                a0, TrustedStack_offset_mcause(csp)
	csrr               a1, mtval
	li                 a2, 0
	cmove              csp, ct0
	j                  .Linvoke_error_handler

.Lset_up_stack_handler:
	// Set up the on-stack context for the callee
	clc                cs1, 0(csp)
	ccleartag          cs1, cs1
	csc                cs1, 0(ct0)
	// Source for context copy.
	cincoffset         ca2, csp, TrustedStack_offset_cra
	// Destination for context copy
	cincoffset         ca3, ct0, TrustedStack_offset_cra
	copyContext        ca3, ca2, cs1, a4

	// Set up the arguments for the call
	cmove              ca0, ct0
	clw                a1, TrustedStack_offset_mcause(csp)
	csrr               a2, mtval
	cmove              csp, ca0

.Linvoke_error_handler:
	// Clear all registers except:
	// cra is set by cjalr.  csp and cgp are needed for the called compartment.
	// ca0, used for the register state
	// ca1, used for mcause
	// ca2, used for mtval
	zeroAllRegistersExcept ra, sp, gp, a0, a1, a2
	// Call the handler.
	cjalr              cra

	// Move the return value to a register that will be cleared in a forced
	// unwind and zero the return registers.
	move               s0, a0
	// Store an error value in return registers, which will be passed to the
	// caller on unwind.
	li                 a0, -1
	li                 a1, 0
	// Return values are 0 for install context, 1 for forced unwind.  Anything
	// that is not either of these is invalid and so we should do a forced
	// unwind anyway.
	bne                s0, zero, .Lforce_unwind

	// We have been asked to install the new register context and resume.
	// We do this by copying the register frame over the save area and entering
	// the exception resume path.  This may fault, but if it does then we will
	// detect it as a double fault and forcibly unwind.

	// Load the trusted stack pointer to ct1
	cspecialr          ct1, mtdc
#ifdef CONFIG_MSHWM
	// Update the spilled copy of the stack high water mark to ensure that we
	// will clear all of the stack used by the error handler and the spilled
	// context.
	csrr               t0, CSR_MSHWM
	csw                t0, TrustedStack_offset_mshwm(ct1)
#endif
	clhu               tp, TrustedStack_offset_frameoffset(ct1)
	addi               tp, tp, -TrustedStackFrame_size
	// ctp points to the current available trusted stack frame.
	cincoffset         ctp, ct1, tp

	// ct0 now contains the export table for the callee
	clc                ct0, TrustedStackFrame_offset_calleeExportTable(ctp)
	cgetbase           s0, ct0
	csetaddr           ct0, ct0, s0
	// ct0 now contains the PCC for the returning compartment.
	clc                ct0, ExportTable_offset_pcc(ct0)
	// This is the *untagged* destination pcc.  Install its address into the
	// real one
	clc                cra, 0(csp)
	cgetaddr           ra, cra
	csetaddr           ct2, ct0, ra
	// Now copy everything else from the stack into the saved context
	// Source
	cincoffset         ca2, csp, TrustedStack_offset_cra
	// Destination
	cincoffset         ca3, ct1, TrustedStack_offset_cra
	copyContext        ca3, ca2, cs1, a4
	// Increment the handler invocation count.  We have now returned and
	// finished touching any data from the error handler that might cause a
	// fault.  Any subsequent fault is not treated as a double fault.  It might
	// be a fault loop, but that will be caught by the fault limit check.
	clh                s1, TrustedStackFrame_offset_errorHandlerCount(ctp)
	addi               s1, s1, 1
	csh                s1, TrustedStackFrame_offset_errorHandlerCount(ctp)

	// Now that the context is set up, let the exception handler code deal with
	// it.  It expects the context to be in csp, so move the context pointer there.
	cmove              csp, ct1
	j                  .Linstall_context

.Lhandle_injected_error:
#ifdef CONFIG_MSHWM
	clw                ra, TrustedStack_offset_mshwm(csp)
	csrw               CSR_MSHWM, ra
	clw                ra, TrustedStack_offset_mshwmb(csp)
	csrw               CSR_MSHWMB, ra
#endif
	j                  .Lhandle_error


	// Value 24 is reserved for custom use.
.Lset_mcause_and_exit_thread:
	csrw               mcause, 24
	// The thread exit code expects the trusted stack pointer to be in csp and
	// the stack pointer to be in mtdc.  After thread exit, we don't need the
	// stack pointer so just put zero there.
	zeroOne            sp
	cspecialrw         csp, mtdc, csp
	j                  .Lthread_exit

	// The continue-resume path expects the location that we will mret to to be
	// in ct2.  If we're just resuming, then resume from the stashed link
	// register value.
.Lreset_mepcc_and_install_context:
	clc                ct2, TrustedStack_offset_mepcc(csp)
	j                  .Linstall_context

	/*
	 * Some switcher instructions' traps are handled specially, by looking at
	 * the offset of mepcc.  Otherwise, we're off to a force unwind.
	 */
.Lhandle_error_in_switcher:
	auipcc             ctp, %cheriot_compartment_hi(.Lswitcher_entry_first_spill)
	cincoffset         ctp, ctp, %cheriot_compartment_lo_i(.Lhandle_error_in_switcher)
	bne                t1, tp, .Lforce_unwind
	li                 a0, -ENOTENOUGHSTACK
	li                 a1, 0

	/*
	 * Cause the interrupted thread to resume as if a return had just executed.
	 * We do this by vectoring to a `cjalr ra` (`cret`) instruction through
	 * `mepcc`; whee!  Overwrites the stored context a0 and a1 with the current
	 * values of those registers, effectively passing them through
	 * .Linstall_context.
	 */
.Linstall_return_context:
	auipcc             ct2, %cheriot_compartment_hi(.Ljust_return)
	cincoffset         ct2, ct2, %cheriot_compartment_lo_i(.Linstall_return_context)
	csc                ca0, TrustedStack_offset_ca0(csp)
	csc                ca1, TrustedStack_offset_ca1(csp)
	j                  .Linstall_context


.size exception_entry_asm, . - exception_entry_asm

/**
 * Pops a frame from the trusted stack.  Leaves all registers in the state
 * expected by the caller of a cross-compartment call, except for the return
 * address which is left in ca2.  The callee is responsible for zeroing
 * argument and temporary registers.
 */
.Lpop_trusted_stack_frame:
	// The below should not fault before returning back to the caller. If a fault occurs there must
	// be a serious bug elsewhere.
	cspecialr          ctp, mtdc
	clear_hazard_slots ctp, ct2
	// make sure there is a frame left in the trusted stack
	clhu               t2, TrustedStack_offset_frameoffset(ctp)
	li                 tp, TrustedStack_offset_frames
	// Move to the previous trusted stack frame.
	addi               t2, t2, -TrustedStackFrame_size
	// If this is the first trusted stack frame, then the csp that we would be
	// loading is the csp on entry, which does not have a spilled area.  In
	// this case, we would fault when loading, so would exit the thread, but we
	// should instead gracefully exit the thread.
	bgeu               tp, t2, .Lset_mcause_and_exit_thread
	cspecialr          ctp, mtdc
	cincoffset         ct1, ctp, t2
	// Restore the stack pointer.  All other spilled values are spilled there.
	clc                csp, TrustedStackFrame_offset_csp(ct1)
	// Update the current frame offset.
	csh                t2, TrustedStack_offset_frameoffset(ctp)
	// Do the loads *after* moving the trusted stack pointer.  In theory, the
	// checks in `check_compartment_stack_integrity` make it impossible for
	// this to fault, but if we do fault here then we'd end up in an infinite
	// loop trying repeatedly to pop the same trusted stack frame.  This would
	// be bad.  Instead, we move the trusted stack pointer *first* and so, if
	// the accesses to the untrusted stack fault, we will detect a fault in the
	// switcher, enter the force-unwind path, and pop the frame for the
	// compartment that gave us a malicious csp.
	clc                cs0, SPILL_SLOT_cs0(csp)
	clc                cs1, SPILL_SLOT_cs1(csp)
	clc                ca2, SPILL_SLOT_pcc(csp)
	clc                cgp, SPILL_SLOT_cgp(csp)
	cincoffset         csp, csp, SPILL_SLOT_SIZE
#ifdef CONFIG_MSHWM
	// read the stack high water mark, which is 16-byte aligned
	// we will use this as base address for stack clearing
	// note that it cannot be greater than stack top as we 
	// we set it to stack top when we pushed to trusted stack frame
	csrr               tp, CSR_MSHWM
#else
	cgetbase           tp, csp
#endif
	cgetaddr           t1, csp
	csetaddr           ct2, csp, tp
	zero_stack         t2, t1, tp
#ifdef CONFIG_MSHWM
	csrw               CSR_MSHWM, sp
#endif
	cret



/*******************************************************************************
 * Switcher-exported library functions.
 *
 * These all provide some reflection on the switcher's state.
 *
 * At the moment, all of these avoid touching any registers except the argument
 * registers, which means that we can define an alternative calling convention
 * for them in the future to allow the compiler to preserve values in the
 * temporary registers across calls.
 *
 * These are all part of the switcher's PCC and so will be covered by the same
 * defence that the switcher has against being made to trap at unexpected
 * times: any trap in the switcher will force unwind the caller's trusted stack
 * frame.  As such, no trap here can leak data.
 *
 * These functions must not use the stack and must ensure that the clobber all
 * registers that hold sensitive state on the way out.
 ******************************************************************************/

// Returns whether the trusted stack has space for N more calls.
	.section .text, "ax", @progbits
	.p2align 2
	.type __Z23trusted_stack_has_spacei,@function
__Z23trusted_stack_has_spacei:
	li                 a2, TrustedStackFrame_size
	mul                a2, a0, a2
	// Load the trusted stack into a register that we will clobber on the way
	// out.
	cspecialr          ca0, mtdc
	clhu               a1, TrustedStack_offset_frameoffset(ca0)
	cgetlen            a0, ca0
	sub                a0, a0, a1
	sltu               a0, a2, a0
	cret

	.section .text, "ax", @progbits
	.p2align 2
	.type __Z22switcher_recover_stackv,@function
__Z22switcher_recover_stackv:
	// Load the trusted stack pointer into a register that we will clobber in
	// two instructions.
	cspecialr          ca0, mtdc
	clhu               a1, TrustedStack_offset_frameoffset(ca0)
	addi               a1, a1, -TrustedStackFrame_size
	cincoffset         ca0, ca0, a1
	clc                ca0, TrustedStackFrame_offset_csp(ca0)
	// If this is the first frame, then the recovered stack will be the stack
	// on entry.  If this is not the first frame then then we need to find the
	// saved CSP from the caller and reset the bounds.  The address of the
	// saved CSP will be the value after the switcher spilled registers and so
	// will be the top of the callee's stack.
	li                 a2, TrustedStack_offset_frames
	beq                a1, a2, 0f

	// Find the previous frame's csp and reset the bounds
	cgetaddr           a1, ca0
	cgetbase           a2, ca0
	sub                a1, a1, a2
	csetaddr           ca0, ca0, a2
	csetboundsexact    ca0, ca0, a1
0:
	cret

	.section .text, "ax", @progbits
	.p2align 2
	.type __Z30trusted_stack_interrupt_threadPv,@function
__Z25switcher_interrupt_threadPv:
	// Load the unsealing key into a register that we will clobber two
	// instructions later.
	LoadCapPCC         ca1, compartment_switcher_sealing_key
	li                 a2, 10
	csetaddr           ca1, ca1, a2
	// The target capability is in ct1. Unseal, check tag and load the entry point offset.
	cunseal            ca1, ca0, ca1
	cgettag            a0, ca1
	// a0 (return register) now contains the tag.  We return false on failure
	// so can just branch to the place where we zero non-return registers from
	// here and it will contain faluse on failure.
	beqz               a0, .Lreturn

	// A thread can't interrupt itself, return failure if it tries.
	cspecialr          ca2, mtdc
	li                 a0, 0
	beq                a2, a1, .Lreturn

	// ca1 now contains the unsealed capability for the target thread.  We
	// allow the target thread to be interrupted if (and only if) the caller is
	// in the same compartment as the interrupted thread.  We will determine
	// this by checking if the base of the two export table entries from the
	// top of the trusted stack frames match.

// Helper macro that loads the export table from the register containing the
// trusted stack.  The two arguments must be different registers.
.macro LoadExportTable result, trustedStack
	clhu               \result, TrustedStack_offset_frameoffset(\trustedStack)
	addi               \result, \result, -TrustedStackFrame_size
	cincoffset         c\result, \trustedStack, \result
	clc                c\result, TrustedStackFrame_offset_calleeExportTable(c\result)
	cgetbase           \result, c\result
.endm

	LoadExportTable    a3, ca1
	cspecialr          ca0, mtdc
	LoadExportTable    a2, ca0

	// ca1 now contains the unsealed capability for the target thread, a3
	// contains the base of the export table entry for that thread, a2 the base
	// of the export table for our thread.
	li                 a0, 42

	// If the two export table entries differ, return.
	bne                a2, a3, .Lreturn
	// After this point, we no longer care about the values in a0, a2, and a3.

	// Mark the thread as interrupted.
	// Store a magic value in mcause
	li                 a2, 25
	csw                a2, TrustedStack_offset_mcause(ca1)
	// Return success
	li                 a0, 1
.Lreturn:
	zeroRegisters      a1, a2, a3
	cret

	.section .text, "ax", @progbits
	.p2align 2
	.type __Z23switcher_current_threadv,@function
__Z23switcher_current_threadv:
	LoadCapPCC         ca0, compartment_switcher_sealing_key
	li                 a1, 10
	csetaddr           ca0, ca0, a1
	cspecialr          ca1, mtdc
	cseal              ca0, ca1, ca0
	li                 a1, 0
	cret

	.section .text, "ax", @progbits
	.p2align 2
	.type __Z28switcher_thread_hazard_slotsv,@function
__Z28switcher_thread_hazard_slotsv:
	// Load the trusted stack pointer into a register that we will clobber in
	// two instructions.
	cspecialr          ca0, mtdc
	clc                ca0, TrustedStack_offset_hazardPointers(ca0)
	cret

	.section .text, "ax", @progbits
	.p2align 2
	.type __Z13thread_id_getv,@function
__Z13thread_id_getv:
	// Load the trusted stack pointer into a register that we will clobber in
	// the next instruction when we load the thread ID.
	cspecialr          ca0, mtdc
	cgettag            a1, ca0
	// If this is a null pointer, don't try to dereference it and report that
	// we are thread 0.  This permits the debug code to work even from things
	// that are not real threads.
	beqz               a1, .Lend
	clh                a0, TrustedStack_offset_threadID(ca0)
.Lend:
	cret


	.section .text, "ax", @progbits
	.p2align 2
	.type __Z25stack_lowest_used_addressv,@function
__Z25stack_lowest_used_addressv:
	// Read the stack high-water mark into the return register.
	csrr               a0, CSR_MSHWM
	cret

	.section .text, "ax", @progbits
	.p2align 2
	.type __Z39switcher_handler_invocation_count_resetv,@function
__Z39switcher_handler_invocation_count_resetv:
	// Trusted stack pointer in ca1
	cspecialr          ca1, mtdc
	// Offset of the current trusted stack frame to a1
	clhu               a0, TrustedStack_offset_frameoffset(ca1)
	addi               a0, a0, -TrustedStackFrame_size
	// Current trusted stack frame to ca1, a0 is dead
	cincoffset         ca1, ca1, a0
	// Current invocation count (for return) in a0
	clh                a0, TrustedStackFrame_offset_errorHandlerCount(ca1)
	// Reset invocation count
	csh                zero, TrustedStackFrame_offset_errorHandlerCount(ca1)
	// Zero trusted stack frame pointer register
	li                 a1, 0
	cret

// The linker expects export tables to start with space for cgp and pcc, then
// the compartment error handler.  We should eventually remove that requirement
// for library export tables, but since they don't consume RAM after loading
// it's not urgent.
	.section	.compartment_export_table,"a",@progbits
export_table_start:
.space 20, 0

/**
 * Helper that exports a switcher function as a library call.
 */
.macro export function, prefix=__library_export_libcalls
	.type	\prefix\function,@object
	.globl	\prefix\function
	.p2align	2
\prefix\function:
	.half	\function-switcher_code_start
	// Number of registers to clear (ignored for library exports)
	.byte	0
	// Interrupts disabled.
	.byte	16
	.size	\prefix\function, 4
.endm

// Switcher entry point must be first.
// We mangle the switcher export as if it were a compartment call.
export __Z26compartment_switcher_entryz, __export_switcher
export __Z23trusted_stack_has_spacei
export __Z22switcher_recover_stackv
export __Z25switcher_interrupt_threadPv
export __Z23switcher_current_threadv
export __Z28switcher_thread_hazard_slotsv
export __Z13thread_id_getv
export __Z25stack_lowest_used_addressv
export __Z39switcher_handler_invocation_count_resetv

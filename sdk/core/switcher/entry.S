// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "export-table-assembly.h"
#include "trusted-stack-assembly.h"
#include "misc-assembly.h"
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
 *
 * Since this is the part of the map labeled "here be dragons", we have added
 * "Register Atlas" comments throughout.  Lines within an atlas consist of a
 * comma-whitespace-separated list of registers, a colon, and descriptive text.
 * In general, atlases should cover all (including dead) registers.  Point
 * changes to the atlas are denoted with "Atlas update", to emphasize that
 * registers not named are not dead but instead retain their meaning from the
 * last full atlas.
 *
 * Labels associated with interesting control flow are annotated with
 *
 *  - "FROM:", which may be repeated once for each predecessor label or these:
 *    - "above": the immediately prior block
 *    - "cross-call": untrusted code making a cross-compartment call
 *    - "malice": untrusted code outside the switcher
 *    - "interrupt": an asynchronous external event
 *    - "error": a trap from within the switcher
 *
 *  - "IFROM:", which indicates an indirect transfer of control (through cjalr
 *    or mepcc/mret, for example).
 *
 *  - "ITO:", the other direction of "IFROM:"
 *
 *  - "IRQ ASSUME:", "any", "deferred" or "enabled".  This declares the state of
 *    the machine, either from explicit instructions or implicit aspects of the
 *    architecture.
 *
 *  - "IRQ REQUIRE:", "any", "deferred" or "enabled".  If not "any", then all
 *    paths into this label must have the indicated IRQ disposition.
 *
 *  - "LIVE IN:", a list of live (in) registers at this point of the code and/or
 *    - "*": the entire general purpose register file (no CSRs or SCRs implied)
 *    - "callee-save": the psABI callee-save registers
 *    - "mcause"
 *    - "mtdc"
 *    - "mtval"
 *    - "mepcc"
 *
 * Control flow instructions may be annotated with "LIVE OUT:" labels.  These
 * capture the subset of live registers meant to be available to the target.
 *
 * For all annotations, optional commentary given in parentheses and may
 * continue onto adjacent lines.
 *
 */
/*
 * Multiple points in the switcher are exposed to callers via sentries (either
 * forward-arc sentries manufactured elsewhere or backwards-arc sentries
 * manufactured by CJALRs herein.  Sentries can have their GL(obal) permission
 * cleared by the bearer, but nothing here relies on PCC being GL(obal): we
 * never store anything derived from our PCC to memory, much less through an
 * authority not bearing SL permission.
 *
 * Similarly, the switcher communicates with the outside world by means of
 * sealed data capabilities (to TrustedStacks and compartment export tables).
 * These, too, can have their GL(obal) bit cleared by bearers, but again, it
 * does not much matter for switcher correctness; see comments marked with
 * "LOCAL SEAL" notes in the code below.
 *
 * We do rely on PCC having L(oad)G(lobal) permission -- which is under seal in
 * sentries and so not mutable by the caller, even if a sentry is loaded
 * through an authority without LG -- so that, in particular, the sealing
 * authorities used herein are GL(obal) and so the sealed capabilities that
 * result are also GL(obal).
 */


switcher_code_start:

# Global for the sealing key.  Stored in the switcher's code section.
	.section .text, "ax", @progbits
	.globl compartment_switcher_sealing_key
	.p2align 3
compartment_switcher_sealing_key:
.Lsealing_key_trusted_stacks:
	.long 0
	.long 0
.Lunsealing_key_import_tables:
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
.macro trustedSpillOne, reg
	csc \reg, TrustedStack_offset_\reg(csp)
.endm

/**
 * Spill all of the registers in the list (in order) to a trusted stack pointed
 * to by csp.
 */
.macro trustedSpillRegisters reg1, regs:vararg
	forall trustedSpillOne, \reg1, \regs
.endm

/// Reload a single register from a trusted stack pointed to by csp.
.macro trustedReloadOne, reg
	clc \reg, TrustedStack_offset_\reg(csp)
.endm

/**
 * Reload all of the registers in the list (in order) to a trusted stack pointed
 * to by csp.
 */
.macro trustedReloadRegisters reg1, regs:vararg
	forall trustedReloadOne, \reg1, \regs
.endm

/**
 * Zero the stack.  The three operands are the base address, the top address,
 * and a scratch register to use.  The base must be a capability but it must
 * be provided without the c prefix because it is used as both a capability
 * and integer register.  All three registers are clobbered but should not be
 * considered safe to expose outside the TCB.
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
 * Clear the hazard pointers associated with this thread.  (See
 * include/stdlib.h:/heap_claim_ephemeral, and its implementation in
 * lib/compartment_helpers/claim_fast.cc for more about hazard pointers.)  We
 * don't care about leaks here (they're store-only from anywhere except the
 * allocator), so just write a 32-bit zero over half of each one to clobber the
 * tags.
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
	 * FROM: cross-call
	 * FROM: malice
	 * IRQ ASSUME: deferred (loader/boot.cc constructs only IRQ-deferring
	 *             sentries to this function; the export entry at the end
	 *             of this file is somewhat fictitious)
	 * LIVE IN: mtdc, ra, sp, gp, s0, s1, t0, t1, a0, a1, a2, a3, a4, a5
	 *          (that is, all registers except tp and t2)
	 *
	 * Atlas:
	 *  mtdc: pointer to this thread's TrustedStack
	 *        (may be 0 from buggy/malicious scheduler thread)
	 *  ra: caller return address
	 *      (at the moment, this is ensured because we enter via an
	 *      IRQ-disabling forward sentry, which requires ra as the destination
	 *      register of the cjalr the caller used, but we are not relying on
	 *      this property, and we hope to relax the switcher's IRQ posture)
	 *  sp: nominally, caller's stack pointer; will check integrity below
	 *  gp: caller state, to be spilled, value unused in switcher
	 *  s0, s1: caller state, to be spilled, value unused in switcher
	 *  t0: possible caller argument to callee, passed or zered in switcher
	 *      (specifically, this is the pointer to arguments beyond a0-a5 and/or
	 *      variadic arguments)
	 *  t1: sealed export table entry for the target callee
	 *      (see LLVM's RISCVExpandPseudo::expandCompartmentCall and, more
	 *      generally, the ABI chapter of the CHERIoT ISA document,
	 *      https://cheriot.org/cheriot-sail/cheriot-architecture.pdf)
	 *  a0, a1, a2, a3, a4, a5: possible caller arguments to callee, passed or
	 *                          zeroed in switcher.
	 *  tp, t2: dead
	 */
	/*
	 * By virtue of making a call, the caller is indicating that all caller-save
	 * registers are dead.  Because we are crossing a trust boundary, the
	 * switcher must spill callee-save registers.  If we find ourselves unable
	 * to do so for "plausibly accidental" reasons, we'll return an error to the
	 * caller (via the exception path; see .Lhandle_error_in_switcher).
	 * Specifically, the first spill here is to the lowest address and so
	 * guaranteed to raise a bounds fault if any of the stores here would access
	 * below the base (lowest address) of the stack capability.
	 *
	 * Certain other kinds of less plausibly accidental malice (for example, an
	 * untagged or sealed or SD-permission-less capability in sp) will also be
	 * caught by this first spill.  In some sense we should forcibly unwind the
	 * caller, but it's acceptable, in the sense that no would-be-callee can be
	 * harmed, to just return an error instead.
	 *
	 * Yet other kinds of less plausibly accidental malice can survive the first
	 * spill.  For example, consider a MC-permission-less capability in sp and a
	 * non-capability value in s0.  While the first spill will not trap, these
	 * forms of malice will certainly be detected in a few instructions, when we
	 * scrutinize sp in detail.  They might (or might not) cause an intervening
	 * (specifically, spill) instruction to trap.  Either way will result in us
	 * ending up in .Lcommon_force_unwind, either directly or via the exception
	 * handler.
	 *
	 * At entry, the register file is safe to expose to the caller, and so if
	 * and when we take the "just return an error" option, no changes, beyond
	 * populating the error return values in a0 and a1, are required.
	 */
	/*
	 * __Z26compartment_switcher_entryz is exposed to callers directly as a
	 * forward-arc interrupt-disabling sentry via the somewhat lengthy chain
	 * of events involving...
	 *   - the .compartment_import_table sections defined in
	 *     compartment.ldscript,
	 *   - the export table defined below (.section .compartment_export_table),
	 *   - firmware.ldscript.in's use of that export table to define
	 *     .switcher_export_table,
	 *   - the firmware image header (loader/types.h's ImgHdr), in particular
	 *     ImgHdr::switcher::exportTable and, again, firmware.ldscript.in's
	 *     use of .switcher_export_table to populate that field, and
	 *   - loader/boot.cc:/populate_imports and its caller's computation of
	 *     switcherPCC.
	 */
	/*
	 * TODO: We'd like to relax the interrupt posture of the switcher where
	 * possible.  Specifically, unless both the caller and callee are running
	 * and to be run with interrupts deferred, we'd like the switcher, and
	 * especially its stack-zeroing, to be preemtable.
	 */
.Lswitch_entry_first_spill:
	/*
	 * FROM: above
	 * ITO: .Lswitch_just_return (via .Lhandle_error_in_switcher)
	 */
	csc               cs0, (SPILL_SLOT_cs0-SPILL_SLOT_SIZE)(csp)
	cincoffset        csp, csp, -SPILL_SLOT_SIZE
	csc               cs1, SPILL_SLOT_cs1(csp)
	csc               cgp, SPILL_SLOT_cgp(csp)
	csc               cra, SPILL_SLOT_pcc(csp)
	/*
	 * Atlas update:
	 *  ra, gp, s0, s1: dead (presently, redundant caller values)
	 */

	/*
	 * Before we access any privileged state, we can verify the compartment's
	 * csp is valid. If not, force unwind.  Note that these checks are purely to
	 * protect the callee, not the switcher itself, which can always bail and
	 * forcibly unwind the caller.
	 *
	 * Make sure the caller's CSP has the expected permissions (including that
	 * it is a stack pointer, by virtue of being local and bearing SL) and that
	 * its top and base are at least 16-byte aligned.  We have already checked
	 * that it is tagged and unsealed and at least 8-byte aligned by virtue of
	 * surviving the stores above.
	 *
	 * TODO for formal verification: it should be the case that after these
	 * tests and the size checks below, no instruction in the switcher
	 * authorized by the capability now in sp can fault.
	 */
//.Lswitch_csp_check:
	cgetperm           t2, csp
	li                 tp, COMPARTMENT_STACK_PERMISSIONS
	bne                tp, t2, .Lcommon_force_unwind
	cgetbase           t2, csp
	or                 t2, t2, sp
	andi               t2, t2, 0xf
	bnez               t2, .Lcommon_force_unwind
	/*
	 * Atlas update:
	 *  t2, tp: dead (again)
	 *  sp: the caller's untrusted stack pointer, now validated and pointing at
	 *      the callee-save register spill area we made above
	 */

	// mtdc should always have an offset of 0.
	cspecialr          ct2, mtdc
	// Atlas update: t2: a pointer to this thread's TrustedStack structure
	/*
	 * This is our first access via mtdc, and so it might trap, if the scheduler
	 * tries a cross-compartment call.  That will be a fairly short trip to an
	 * infinite loop (see commentary in exception_entry_asm).
	 */
	clear_hazard_slots /* trusted stack = */ ct2, /* scratch = */ ctp
	// Atlas update: tp: dead (again)

//.Lswitch_trusted_stack_push:
	/*
	 * TrustedStack::frames[] is a flexible array member at the end of the
	 * structure, and the stack of frames it represents grows *upwards* (with
	 * [0] the initial activation, [1] the first cross-compartment call, and so
	 * on).  Thus, if the frame offset points "one past the end" (or futher
	 * out), we have no more frames available, so off we go to
	 * .Lswitch_trusted_stack_exhausted .
	 */
	clhu               tp, TrustedStack_offset_frameoffset(ct2)
	cgetlen            s0, ct2
	/*
	 * Atlas update:
	 *  s0: scalar length of the TrustedStack structure
	 *  tp: scalar offset of the next available TrustedStack::frames[] element
	 */
	// LIVE OUT: mtdc, sp
	bgeu               tp, s0, .Lswitch_trusted_stack_exhausted
	// Atlas update: s0: dead
	// we are past the stacks checks.
	cincoffset         ctp, ct2, tp
	// Atlas update: tp: pointer to the next available TrustedStackFrame
	/*
	 * Populate that stack frame by...
	 * 1. spilling the caller's stack pointer, as modified by the spills at the
	 *    start of this function.
	 */
	csc                csp, TrustedStackFrame_offset_csp(ctp)
	/*
	 * 2. zeroing the number of error handler invocations (we have just entered
	 *    this call, so no faults triggered during this call yet).
	 */
	csh                zero, TrustedStackFrame_offset_errorHandlerCount(ctp)
	/*
	 * 3. For now, store a null export entry.  This is largely cosmetic; we will
	 *    not attempt to access this value before it is set to the real export
	 *    table entry below.  Should we trap, the logic at
	 *    .Lhandle_error_switcher_pcc will cause us to force unwind, popping
	 *    this frame before any subsequent action.
	 *
	 *    TODO for formal verification: prove that this store is dead and can
	 *    be eliminated.
	 */
	csc                cnull, TrustedStackFrame_offset_calleeExportTable(ctp)
	/*
	 * Update the frame offset, using s1 to hold a scratch scalar.  Any fault
	 * before this point (wrong target cap, unaligned stack, etc.) is seen as a
	 * fault in the caller. After writing the new TrustedSstack::frameoffset,
	 * any fault is seen as a callee fault.
	 */
	clhu               s1, TrustedStack_offset_frameoffset(ct2)
	addi               s1, s1, TrustedStackFrame_size
	csh                s1, TrustedStack_offset_frameoffset(ct2)

	/*
	 * Chop off the stack, using...
	 *  - s0 for the current untrusted stack base address (the lowest address of
	 *    the register spill we created at .Lswitch_entry_first_spill)
	 *  - s1 for the length of the stack suffix to which the callee is entitled
	 */
//.Lswitch_stack_chop:
	cgetaddr           s0, csp
	cgetbase           s1, csp
	csetaddr           csp, csp, s1
	sub                s1, s0, s1
	csetboundsexact    ct2, csp, s1
	csetaddr           csp, ct2, s0
	/*
	 * Atlas:
	 *  s0: address of stack boundary between caller and callee frames, that is,
	 *      the lowest address of the register spill from
	 *      .Lswitch_entry_first_spill)
	 *  sp: pointer to stack, with its limit and address set to the address in
	 *      s0.  The base and permissions have not been altered from sp at
	 *      entry, and the tag remains set since all manipulations have been
	 *      monotone non-increasing of, and within, bounds.
	 *  tp: pointer to the freshly populated TrustedStackFrame (still)
	 *  t1: sealed export table entry for the target callee (still)
	 *  a0, a1, a2, a3, a4, a5, t0: call argument values / to be zeroed (still)
	 *  t2, s1: dead (again)
	 *  ra, gp: dead (still)
	 */
#ifdef CONFIG_MSHWM
	// Read the stack high water mark (which is 16-byte aligned)
	csrr               gp, CSR_MSHWM
	// Skip zeroing if high water mark >= stack pointer
//.Lswitch_shwm_skip_zero:
	bge                gp, sp, .Lswitch_after_zero
	/*
	 * Use stack high water mark as base address for zeroing.  If this faults
	 * then it will trigger a force unwind.  This can happen only if the caller
	 * is doing something bad.
	 */
	csetaddr           ct2, csp, gp
#endif
	zero_stack         /* base = */ t2, /* top = */ s0, /* scratch = */ gp
.Lswitch_after_zero:
	/*
	 * FROM: above
	 * FROM: .Lswitch_shwm_skip_zero
	 * LIVE IN: mtdc, sp, tp, t0, t1, a0, a1, a2, a3, a4, a5
	 *
	 * Atlas:
	 *  sp: pointer to stack, with bounds as t2, cursor at boundary in s0
	 *      (still)
	 *  tp: pointer to the freshly populated TrustedStackFrame (still)
	 *  t1: sealed export table entry for the target callee (still)
	 *  a0, a1, a2, a3, a4, a5, t0: call argument values / to be zeroed (still)
	 *  ra, s1: dead (still)
	 *  s0, t2, gp: dead (again)
	 */

	// Fetch the sealing key
	LoadCapPCC         cs0, .Lunsealing_key_import_tables
	// Atlas update: s0: switcher sealing key
	/*
	 * The caller's handle to the callee (the sealed capability to the export
	 * table entry) is in t1, which has been kept live all this time.  Unseal
	 * and load the entry point offset.
	 */
//.Lswitch_unseal_entry:
	cunseal            ct1, ct1, cs0
	/*
	 * Atlas update:
	 *  t1: if tagged, an unsealed pointer with bounds encompassing callee
	 *      compartment ExportTable and ExportEntry array and cursor pointing at
	 *      the callee ExportEntry; if untagged, the caller is malicious or
	 *      deeply confused, the next instruction will trap, and we'll
	 *      .Lcommon_force_unwind via exception_entry_asm and
	 *      .Lhandle_error_in_switcher.
	 */
	/*
	 * LOCAL SEAL: If it happened that the export table reference given to us
	 * is not GL(obal), then the result of unsealing above, now in t1, will
	 * also be not GL(obal).  This reference is stored to the TrustedStack frame
	 * through a SL-bearing authority (because the TrustedStack also holds our
	 * register spill area, and so must have SL) but neither it or any monotone
	 * progeny otherwise escape the switcher's private register file.
	 */
	/*
	 * Load the entry point offset.  If cunseal failed then this will fault and
	 * we will force unwind; see .Lhandle_error_switcher_pcc_check.
	 */
	clhu               s0, ExportEntry_offset_functionStart(ct1)
	// Atlas update: s0: callee compartment function entrypoint offset
	/*
	 * At this point, we know that the cunseal has succeeded (we didn't trap on
	 * the load) and so it's safe to store the unsealed value of the export
	 * table pointer.
	 *
	 * TODO for formal verification: Nothing between this point and transition
	 * to the callee should fault.
	 */
	csc                ct1, TrustedStackFrame_offset_calleeExportTable(ctp)

//.Lswitch_stack_check_length:
	/*
	 * Load the minimum stack size required by the callee, clobbering tp, which
	 * holds a capability to the TrustedStackFrame, bringing us closer to a
	 * register file that is not holding values kept secret from the callee.
	 */
	clbu               tp, ExportEntry_offset_minimumStackSize(ct1)
	// Atlas update: tp: minimum stack size, in units of 8 bytes.
	slli               tp, tp, 3
	// Atlas update: tp: minimum stack size, in bytes.
	/*
	 * Check that the stack is large enough for the callee.
	 * At this point, we have already truncated the stack and so the length of
	 * the stack is the length that the callee can use.
	 */
	cgetlen            t2, csp
	// Atlas update: t2: length of available stack
	/*
	 * Include the space we reserved for the unwind state.
	 *
	 * tp holds the number of required stack bytes, a value between 0 and 0x7F8
	 * (the result of an unsigned byte load left shifted by 3).  Given this
	 * extremely limited range, adding STACK_ENTRY_RESERVED_SPACE will not cause
	 * overflow (while instead subtracting it from the available length, in t2,
	 * might underflow).
	 *
	 * TODO for formal verification: prove the above.
	 */
	addi               tp, tp, STACK_ENTRY_RESERVED_SPACE
	// LIVE OUT: mtdc
	bgtu               tp, t2, .Lswitch_stack_too_small

	/*
	 * Reserve space for unwind state and so on; this cannot take sp out of
	 * bounds, in light of the check we just performed.
	 */
	cincoffset 	       csp, csp, -STACK_ENTRY_RESERVED_SPACE
#ifdef CONFIG_MSHWM
	// store new stack top as stack high water mark
	csrw               CSR_MSHWM, sp
#endif

	// Get the flags field into tp
	clbu               tp, ExportEntry_offset_flags(ct1)
	// Atlas update: tp: callee entry flags field

	// All ExportEntry state has been consulted; move to ExportTable header
	cgetbase           s1, ct1
	csetaddr           ct1, ct1, s1
	/*
	 * Atlas update:
	 *  t1: pointer to the callee compartment ExportTable structure.  Bounds
	 *      still inclusive of ExportEntry array, but that will not be accessed.
	 */
//.Lswitch_callee_load:
	// At this point we begin loading callee compartment state.
	clc                cgp, ExportTable_offset_cgp(ct1)
	// Atlas update: gp: target compartment CGP
	clc                cra, ExportTable_offset_pcc(ct1)
	cincoffset         cra, cra, s0
	// Atlas update: ra: target function entrypoint (pcc base + offset from s0)

	// Zero any unused argument registers
	/*
	 * The low 3 bits of the flags field (tp) contain the number of argument
	 * registers to pass.  We create a small sled that zeroes them in the order
	 * they are used as argument registers, and we jump into the middle of it at
	 * an offset defined by that value, preserving the prefix of the sequence.
	 */
.Lswitch_load_zero_arguments_start:
	// FROM: above
	auipcc             cs0, %cheriot_compartment_hi(.Lswitch_zero_arguments_start)
	cincoffset         cs0, cs0, %cheriot_compartment_lo_i(.Lswitch_load_zero_arguments_start)
	// Atlas update: s0: .Lzero_arguments_start
	andi               t2, tp, 0x7 // loader/types.h's ExportEntry::flags
	/*
	 * Change from the number of registers to pass into the number of 2-byte
	 * instructions to skip.
	 */
	sll                t2, t2, 1
	// Offset the jump target by the number of instructions to skip
	cincoffset         cs0, cs0, t2
	// Jump into the sled.
	cjr                cs0
.Lswitch_zero_arguments_start:
	// IFROM: above
	zeroRegisters      a0, a1, a2, a3, a4, a5, t0

	/*
	 * Enable interrupts if the interrupt-disable bit is not set in flags.  See
	 * loader/types.h's InterruptStatus and ExportEntry::InterruptStatusMask.
	 * InterruptStatus::Inherited is prohibited on export entries, so we need
	 * look only at one bit.
	 */
	andi               t1, tp, ExportEntryInterruptStatusSwitcherMask
	bnez               t1, .Lswitch_skip_interrupt_enable
	csrsi              mstatus, 0x8
.Lswitch_skip_interrupt_enable:
	/*
	 * FROM: above
	 * IRQ REQUIRE: any (have adopted callee disposition)
	 *
	 * Atlas:
	 *  ra: (still) target function entrypoint
	 *  sp: (still) pointer to stack, below compartment invocation local storage
	 *  gp: (still) target compartment CGP
	 *  a0, a1, a2, a3, a4, a5, t0: arguments or zeroed, as above
	 *  tp, t1, t2, s0, s1: dead
	 */
	/*
	 * There is an interesting narrow race to consider here.  We're preemptable
	 * and in the switcher.  That means someone could call
	 * __Z25switcher_interrupt_threadPv on us, and when we came back on core,
	 * we'd jump ahead to switcher_after_compartment_call, via...
	 *
	 *   - .Lexception_scheduler_return_installed
	 *   - .Lhandle_injected_error
	 *   - .Lhandle_error
	 *   - .Lhandle_error_switcher_pcc
	 *   - .Lhandle_error_in_switcher
	 *   - .Lcommon_force_unwind
	 *
	 * That is, rather than invoking the callee's compartment's error handler,
	 * and letting it service the MCAUSE_THREAD_INTERRUPT, we'll return to the
	 * caller with -ECOMPARTMENTFAIL.
	 *
	 * TODO: https://github.com/CHERIoT-Platform/cheriot-rtos/issues/372
	 */
	/*
	 * Up to 10 registers are carrying state for the callee or are properly
	 * zeroed.  Clear the remaining 5 now.
	 */
//.Lswitch_caller_dead_zeros:
	zeroRegisters      tp, t1, t2, s0, s1
//.Lswitch_callee_call:
	/*
	 * "cjalr cra" simultaneously moves the live-in ra value into the *next*
	 * program counter and the program counter (of the instruction itself) into
	 * ra (while sealing it to be a backwards-arc sentry).  That is, the value
	 * we have so carefully been keeping in ra is clobbered, but only after it
	 * becomes the next program counter.
	 */
	// LIVE OUT: *
	cjalr              cra

	.globl switcher_after_compartment_call
switcher_after_compartment_call:
	/*
	 * FROM: malice
	 * IFROM: above
	 * FROM: .Lswitch_stack_too_small
	 * FROM: .Lcommon_force_unwind
	 * IRQ ASSUME: any (both IRQ-deferring and IRQ-enabling sentries are
	 *             provided to the callees and can escape for malice's use, and
	 *             the TrustedStack spill frame is not precious, and nothing
	 *             that would happen were we are preempted would shift our
	 *             TrustedStack::frameoffset or the contents of ::frames)
	 * LIVE IN: mtdc, a0, a1
	 *
	 * Atlas:
	 *  mtdc: pointer to this thread's TrustedStack
	 *  a0, a1: return value(s).  The callee function must ensure that it clears
	 *          these as appropriate if it is returning 0 or 1 values and not 2.
	 *  ra, sp, gp: dead or callee state (to be replaced by caller state)
	 *  tp, s0, s1, t0, t1, t2, a2, a3, a4, a5: dead or callee state (to be
	 *                                          zeroed before return to caller)
	 */
	/*
	 * Pop a frame from the trusted stack, leaving all registers in the state
	 * expected by the caller of a cross-compartment call.  The callee is
	 * responsible for zeroing unused return registers; the switcher will zero
	 * other non-return argument and temporary registers.
	 *
	 * This unwind path is common to both ordinary return (from above), benign
	 * errors after we'd set up the trusted frame (.Lswitch_stack_too_small),
	 * and forced unwinds (.Lcommon_force_unwind).
	 *
	 * TODO for formal verification: the below should not fault before returning
	 * back to the caller. If a fault occurs there must be a serious bug
	 * elsewhere.
	 */
	/*
	 * As just before the call, we are preemptive and in the switcher.  If we
	 * are signaled via MCAUSE_THREAD_INTERRUPT at this point, we will come
	 * back here (with a0 holding -ECOMPARTMENTFAIL and a1 holding 0).  This
	 * block is _idempotent_ until the update of mtdc's
	 * TrustedStack::frameoffset, so until then we will effectively just
	 * clobber the return values.  After that, though, we'd forcibly unwind out
	 * of the caller.
	 *
	 * TODO: https://github.com/CHERIoT-Platform/cheriot-rtos/issues/372
	 */
	/*
	 * The return sentry given to the callee as part of that cjalr could be
	 * captured by the callee or passed between compartments arbitrarily for
	 * later use.  That is, in some sense, we cannot assume that any use of this
	 * sentry corresponds to the most recent derivation of it by this thread.
	 * Phrased differently, the sentry created by the "cjalr" above is not tied
	 * to the topmost TrustedStackFrame at the time of its creation.  Invoking
	 * this sentry, regardless of how one comes to hold it, and even if
	 * invocation is not matched to the call that constructed any given instance
	 * of it, will always result in popping the topmost trusted stack frame (at
	 * the time of invocation) and returning to its caller.  Thus, the
	 * possibility of more than one of these sentries in scope at any moment is
	 * not concerning.
	 *
	 * Additionally, threads are given a manufactured, interrupt-deferring
	 * sentry to here as part of their initial activation frame (so that
	 * returning acts as an orderly unwind).  See
	 * loader/boot.cc:/boot_threads_create .
	 *
	 * Being robust to malicious or "unusual" entry here is facilitated by the
	 * requirements of the next block of code being minimal: mtdc must be a
	 * TrustedStack pointer.  The contents of a0 and a1 will be exposed to the
	 * compartment above the one currently executing, or the thread will be
	 * terminated if there is no such.
	 */

	cspecialr          ctp, mtdc
	// Atlas update: tp: pointer to TrustedStack

	clear_hazard_slots ctp, ct2

	/*
	 * Make sure there is a frame left in the trusted stack by...
	 *
	 * 1. Loading TrustedStack::frameoffset and offsetof(TrustedStack, frames)
	 */
	clhu               t2, TrustedStack_offset_frameoffset(ctp)
	li                 t0, TrustedStack_offset_frames
	/*
	 * 2. Decreasing frameoffset by one frame.  This will go below
	 *    offsetof(TrustedStack, frames) if there are no active frames.
	 */
	addi               t2, t2, -TrustedStackFrame_size
	/*
	 * 3. Comparing.  If this is the first trusted stack frame, then the csp
	 * that we would be loading is the csp on entry, which does not have a
	 * spilled area.  In this case, we would fault when loading (because the
	 * stack cursor is at its limit), so would exit the thread, but we should
	 * instead gracefully exit the thread.
	 */
	bgeu               t0, t2, .Lcommon_defer_irqs_and_thread_exit
	cincoffset         ct1, ctp, t2
	/*
	 * Atlas update:
	 *  t0: dead (again)
	 *  t1: pointer to the TrustedStackFrame to bring on core
	 *  t2: the TrustedStack::frameoffset associated with t1
	 */

	/*
	 * Restore the untrusted stack pointer from the trusted stack.  This points
	 * at the spill frame, created by .Lswitch_entry_first_spill and following
	 * instructions, holding caller register values.
	 */
	clc                csp, TrustedStackFrame_offset_csp(ct1)
	/*
	 * Atlas update:
	 *  sp: pointer to untrusted stack (the spill frame created by
	 *      .Lswitch_entry_first_spill)
	 */
	// Update the current frame offset in the TrustedStack
	csh                t2, TrustedStack_offset_frameoffset(ctp)
	/*
	 * Do the loads *after* moving the trusted stack pointer.  In theory, the
	 * checks after `.Lswitch_entry_first_spill` make it impossible for this to
	 * fault, but if we do fault here and hadn't moved the frame offset, then
	 * we'd end up in an infinite loop trying repeatedly to pop the same
	 * trusted stack frame.  This would be bad.  Instead, we move the trusted
	 * stack pointer *first* and so, if the accesses to the untrusted stack
	 * fault, we will detect a fault in the switcher, enter the force-unwind
	 * path, and pop the frame for the compartment that gave us a malicious
	 * csp.
	 */
	clc                cs0, SPILL_SLOT_cs0(csp)
	clc                cs1, SPILL_SLOT_cs1(csp)
	clc                cra, SPILL_SLOT_pcc(csp)
	clc                cgp, SPILL_SLOT_cgp(csp)
	cincoffset         csp, csp, SPILL_SLOT_SIZE
#ifdef CONFIG_MSHWM
	/*
	 * Read the stack high water mark, which is 16-byte aligned.  We will use
	 * this as base address for stack clearing.  Note that it cannot be greater
	 * than stack top as we set it to stack top when we pushed to the trusted
	 * stack frame, and it is a monotonically non-increasing value.
	 */
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

	// Zero all registers not holding state intended for caller; see atlas below
.Lswitch_callee_dead_zeros:
	/*
	 * FROM: above
	 * FROM: .Lswitch_trusted_stack_exhausted
	 * LIVE IN: mtdc, ra, sp, gp, s0, s1, a0, a1
	 *
	 * Atlas:
	 *  mtdc: pointer to this thread's TrustedStack
	 *  a0, a1: return value(s)
	 *  ra, sp, gp, s0, s1: caller state
	 *  tp, t0, t1, t2, a2, a3, a4, a5: dead (to be zeroed here)
	 */
	zeroAllRegistersExcept ra, sp, gp, s0, s1, a0, a1
.Lswitch_just_return:
	/*
	 * FROM: above
	 * IFROM: .Lswitch_entry_first_spill (via .Lhandle_error_in_switcher)
	 * LIVE IN: mtdc, ra, sp, gp, s0, s1, a0, a1
	 *
	 * Atlas:
	 *  mtdc: pointer to this thread's TrustedStack
	 *  a0, a1: return value(s) (still)
	 *  ra, sp, gp, s0, s1: caller state
	 *  tp, t0, t1, t2, a2, a3, a4, a5: zero (if from above) or caller state (if
	 *                                  from .Lhandle_error_in_switcher via
	 *                                  .Lhandle_return_context_install)
	 */
	cret

	/*
	 * If the stack is too small, we don't do the call, but to avoid leaking
	 * any other state we still go through the same return path as normal.  We
	 * set the return registers to -ENOTENOUGHSTACK and 0, so users can see
	 * that this is the failure reason.
	 */
.Lswitch_stack_too_small:
	/*
	 * FROM: .Lswitch_stack_check_length
	 * IRQ REQUIRE: any (TrustedStack spill frame is not precious)
	 * LIVE IN: mtdc
	 *
	 * Atlas:
	 *  mtdc: thread trusted stack pointer
	 */
	li                 a0, -ENOTENOUGHSTACK
	li                 a1, 0
	// Atlas update: a0, a1: error return values
	// LIVE OUT: mtdc, a0, a1
	j                  switcher_after_compartment_call

	/*
	 * If we have run out of trusted stack, then just restore the caller's state
	 * (mostly, the callee-save registers from the spills we did at the top of
	 * __Z26compartment_switcher_entryz) and return an error value.
	 */
.Lswitch_trusted_stack_exhausted:
	/*
	 * FROM: .Lswitch_trusted_stack_push
	 * IRQ REQUIRE: any (all state is in registers, TrustedStack spill frame is
	 *              not precious)
	 * LIVE IN: mtdc, sp
	 *
	 * Atlas:
	 *  mtdc: TrustedStack pointer
	 *  sp: Caller stack pointer, pointing at switcher spill frame, after
	 *      validation
	 */
	/*
	 * Restore the spilled values.  Because csp has survived being spilled to
	 * and the permission validations, these will not fault.
	 */
	clc                cs0, SPILL_SLOT_cs0(csp)
	clc                cs1, SPILL_SLOT_cs1(csp)
	clc                cra, SPILL_SLOT_pcc(csp)
	clc                cgp, SPILL_SLOT_cgp(csp)
	cincoffset         csp, csp, SPILL_SLOT_SIZE
	// Set the first return register (a0) and zero the other (a1)
	li                 a0, -ENOTENOUGHTRUSTEDSTACK
	zeroOne            a1
	j                  .Lswitch_callee_dead_zeros

.size compartment_switcher_entry, . - compartment_switcher_entry

	.global  exception_entry_asm
	.p2align 2
/**
 * The entry point of all exceptions and interrupts
 *
 * For now, the entire routine is run with interrupts disabled.
 */
exception_entry_asm:
	/*
	 * FROM: malice
	 * FROM: interrupt
	 * FROM: error
	 * IRQ ASSUME: deferred (sole entry is via architectural exception path,
	 *             which unconditionally, atomically defers IRQs)
	 * LIVE IN: mcause, mtval, mtdc, *
	 *
	 * Atlas:
	 *  mtdc: either pointer to TrustedStack or zero
	 *  mcause, mtval: architecture-specified exception information.  These are
	 *                 assumed correct -- for example, that it is impossible for
	 *                 untrusted code to enter the exception path with
	 *                 arbitrarily chosen values.
	 *  *: The GPRs at the time of exception.
	 */
	/*
	 * We do not trust the interruptee's context. We cannot use its stack in any
	 * way.  The save register frame we can use is fetched from the
	 * TrustedStack.  In general, mtdc holds the trusted stack register.  We are
	 * here with interrupts off and precious few registers available to us, so
	 * swap it with the csp (we'll put it back, later).
	 */
	cspecialrw         csp, mtdc, csp

	/*
	 * If we read out zero, we've reentered the exception and are about to trap
	 * (in spillRegisters, which uses sp as its authority).
	 *
	 * Failure to guard here would mean that the trap in spillRegisters below
	 * would re-enter the trap-handler with an unknown value (the first trap's
	 * sp) in mtdc, which the rest of this code would take to be a valid
	 * TrustedStack.  Exactly what would happen then is hard to say; we'd try
	 * spilling registers to a potentially attacker-controlled pointer, at the
	 * very least, and that's something to avoid.
	 */
	beqz               sp, .Lexception_reentered

	/*
	 * The guest sp/csp (x2/c2) is now in mtdc. Will be spilled later, but we
	 * spill all the other 14 registers now.
	 */
	trustedSpillRegisters     cra, cgp, ctp, ct0, ct1, ct2, cs0, cs1, ca0, ca1, ca2, ca3, ca4, ca5

	/*
	 * The control flow of an exiting thread rejoins us (that is, running
	 * threads which have taken an exception, be that a trap or an interrupt)
	 * here, as if it had taken an exception.  We even use the mcause register
	 * to signal the exit "exception"; see .Lcommon_thread_exit.
	 */
.Lexception_exiting_threads_rejoin:
	/*
	 * FROM: above
	 * FROM: .Lcommon_thread_exit
	 * IRQ REQUIRE: deferred (about to set MTDC to nullptr)
	 * LIVE IN: mcause, mtval, mtdc, sp
	 *
	 * Atlas:
	 *  mtdc: the interrupted context's sp (or zero, if coming from
	 *        .Lcommon_thread_exit)
	 *  sp: TrustedStack pointer (and in particular a spill frame we can use)
	 */

	/*
	 * mtdc got swapped with the thread's csp, store it and clobber mtdc with
	 * zero (using t1 as a scratch register, because using source register index
	 * 0 with cspecialrw means "don't write" rather than "write zero").  The
	 * trusted stack pointer is solely in csp, now; if we take another trap
	 * before a new one is installed, or if the scheduler enables interrupts and
	 * we take one, we'll pull this zero out of mtdc, above.
	 */
	zeroOne            t1
	cspecialrw         ct1, mtdc, ct1
	csc                ct1, TrustedStack_offset_csp(csp)
	/*
	 * Atlas update:
	 *  mtdc: zero
	 *  sp: (still) TrustedStack pointer
	 */

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
	// Atlas update: t1: copy of mcause

	/*
	 * If we hit one of the exception conditions that we should let compartments
	 * handle then maybe deliver it to the compartment (if it has a handler that
	 * we have the resources to invoke).
	 */
//.Lexception_might_handle:
	li                 a0, MCAUSE_CHERI
	// LIVE OUT: sp
	beq                a0, t1, .Lhandle_error
	/*
	 * A single test suffices to catch all of...
	 *  - MCAUSE_INST_MISALINED (0),
	 *  - MCAUSE_INST_ACCESS_FAULT (1),
	 *  - MCAUSE_ILLEGAL_INSTRUCTION (2),
	 *  - MCAUSE_BREAKPOINT (3),
	 *  - MCAUSE_LOAD_MISALIGNED (4),
	 *  - MCAUSE_LOAD_ACCESS_FAULT (5),
	 *  - MCAUSE_STORE_MISALIGNED (6),
	 *  - MCAUSE_STORE_ACCESS_FAULT (7)
	 */
	li                 a0, 0x8
	// LIVE OUT: sp
	bltu               t1, a0, .Lhandle_error

//.Lexception_scheduler_call:
	// TODO: On an ecall, we don't need to save any caller-save registers

	/*
	 * At this point, thread state is completely saved. Now prepare the
	 * scheduler context.
	 * Function signature of the scheduler entry point:
	 * TrustedStack *exception_entry(TrustedStack *sealedTStack,
	 *     size_t mcause, size_t mepc, size_t mtval)
	 */
	LoadCapPCC         ca0, .Lsealing_key_trusted_stacks
	cseal              ca0, csp, ca0 // sealed trusted stack
	mv                 a1, t1 // mcause
	cgetaddr           a2, ct0 // mepcc address
	csrr               a3, mtval
	// Fetch the stack, cgp and the trusted stack for the scheduler.
	LoadCapPCC         csp, switcher_scheduler_entry_csp
	LoadCapPCC         cgp, switcher_scheduler_entry_cgp
	LoadCapPCC         cra, switcher_scheduler_entry_pcc
	/*
	 * Atlas:
	 *  ra, gp: scheduler compartment context
	 *  sp: scheduler thread context
	 *  a0: sealed trusted stack pointer (opaque thread handle)
	 *  a1: copy of mcause
	 *  a2: copy of mepc
	 *  a3: copy of mtval
	 *  tp, t0, t1, t2, s0, s1, a4, a5: dead
	 */

	// Zero everything apart from things explicitly passed to scheduler.
	zeroAllRegistersExcept ra, sp, gp, a0, a1, a2, a3

	// Call the scheduler.  This returns the new thread in ca0.
	cjalr              cra

//.Lexception_scheduler_return:
	/*
	 * IFROM: above
	 * IRQ ASSUME: deferred (reachable only by IRQ-deferring reverse sentry)
	 * IRQ REQUIRE: deferred (mtdc is zero)
	 * LIVE IN: a0
	 *
	 * Atlas:
	 *  mtdc: (still) zero
	 *  a0: sealed trusted stack pointer to bring onto core
	 */
	/*
	 * The interrupts-disabling return sentry handed to the scheduler as part of
	 * that cjalr may be captured on its stack, but as the scheduler is the
	 * topmost and only compartment in its thread (as it cannot make
	 * cross-compartment calls without faulting, due to the null presently in
	 * mtdc), there is very little that can go wrong as as a result of that
	 * capture.
	 */
	/*
	 * The scheduler may change interrupt posture or may trap (and infinite loop
	 * if it does so; see the top of exception_entry_asm and recall that mtdc is
	 * 0 at this point), but if it returns to us (that is, we reach here), the
	 * use of the sentry created by cjalr will have restored us to deferring
	 * interrupts, and we will remain in that posture until the mret in
	 * .Lcommon_context_install.
	 */

	// Switch onto the new thread's trusted stack
	LoadCapPCC         csp, .Lsealing_key_trusted_stacks
	cunseal            csp, ca0, csp
	// Atlas update: sp: unsealed target thread trusted stack pointer
	/*
	 * LOCAL SEAL: if the scheduler has shed GL(obal) of the reference it gave
	 * us in a0, then sp will also lack GL(obal) after unsealing.  This
	 * reference is not stored in memory (in the switcher, anyway), just mtdc.
	 * However, when this reference is extracted and sealed for the next
	 * context switch (in .Lexception_scheduler_call), the result will lack
	 * GL(obal), which will likely prove challenging for the scheduler.  That
	 * is, this is an elaborate way for the scheduler to crash itself.
	 */

	clw                t0, TrustedStack_offset_mcause(csp)
	// Atlas update: t0: stored mcause for the target thread

	/*
	 * Only now that we have done something that actually requires the tag of
	 * csp be set, put it into mtdc.  If the scheduler has returned something
	 * untagged or something with the wrong otype, the cunseal will have left
	 * csp untagged and clw will trap with mtdc still 0.  If we made it here,
	 * though, csp is tagged and so was tagged and correctly typed, and so it
	 * is safe to install it to mtdc.  We won't cause traps between here and
	 * mret, so reentrancy is no longer a concern.
	 */
	cspecialw          mtdc, csp
//.Lexception_scheduler_return_installed:
	/*
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * Atlas update: mtdc: TrustedStack pointer
	 */

	/*
	 * If mcause is MCAUSE_THREAD_INTERRUPT, then we will jump into the error
	 * handler: another thread has signalled that this thread should be
	 * interrupted.  MCAUSE_THREAD_INTERRUPT is a reserved exception number that
	 * we repurpose to indicate explicit interruption.
	 */
	li                 t1, MCAUSE_THREAD_INTERRUPT
	// LIVE OUT: mtdc, sp
	beq                t0, t1, .Lhandle_injected_error

	/*
	 * Environment call from M-mode is exception code 11.
	 * We need to skip the ecall instruction to avoid an infinite loop.
	 */
	li                 t1, 11
	clc                ct2, TrustedStack_offset_mepcc(csp)
	// Atlas update: t2: interrupted program counter to resume
	// LIVE OUT: mtdc, sp, t2
	bne                t0, t1, .Lcommon_context_install
	cincoffset         ct2, ct2, 4
	// Fall through to install context

.Lcommon_context_install:
	/*
	 * FROM: above
	 * FROM: .Lhandle_error_install_context
	 * FROM: .Lhandle_return_context_install
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: mtdc, sp, t2
	 *
	 * Atlas:
	 *  mtdc, sp: TrustedStack pointer
	 *  t2: target pcc to resume
	 *  ra, gp, tp, t0, t1, s0, s1, a0, a1, a2, a3, a4, a5: dead
	 */
	/*
	 * All registers other than sp and t2 are in unspecified states and will be
	 * overwritten when we install the context.
	 */
	clw                ra, TrustedStack_offset_mstatus(csp)
	csrw               mstatus, ra
#ifdef CONFIG_MSHWM
	clw                ra, TrustedStack_offset_mshwm(csp)
	csrw               CSR_MSHWM, ra
	clw                ra, TrustedStack_offset_mshwmb(csp)
	csrw               CSR_MSHWMB, ra
#endif
	cspecialw          mepcc, ct2

	/*
	 * reloadRegisters restores registers in the order given, and we ensure that
	 * sp/csp (x2/c2) will be loaded last and will overwrite the trusted stack
	 * pointer with the thread's stack pointer.
	 */
	trustedReloadRegisters cra, cgp, ctp, ct0, ct1, ct2, cs0, cs1, ca0, ca1, ca2, ca3, ca4, ca5, csp
	mret

/**
 * We are starting a forced unwind.  This is reached either when we are unable
 * to run an error handler, or when we do run an error handler and it instructs
 * us to return.  This treats all register values as undefined on entry.
 */
.Lcommon_force_unwind:
	/*
	 * FROM: .Lhandle_error_handler_return_irqs
	 * FROM: .Lhandle_error_in_switcher
	 * FROM: .Lhandle_error_test_double_fault
	 * FROM: .Lhandle_error_test_too_many
	 * FROM: .Lhandle_error_try_stackless
	 * FROM: .Lswitch_csp_check
	 * IRQ REQUIRE: any
	 * LIVE IN: mtdc
	 *
	 * Atlas:
	 *  mtdc:  pointer to TrustedStack
	 */
	li                 a0, -ECOMPARTMENTFAIL
	li                 a1, 0
	j                  switcher_after_compartment_call

/**
 * If we have a possibly recoverable error, see if we have a useful error
 * handler.  At this point, the register state will have been saved in the
 * register-save area and so we just need to set up the environment.
 * The handler will have this type signature:
 *
 * enum ErrorRecoveryBehaviour
 * compartment_error_handler(struct ErrorState *frame,
 *                           size_t             mcause,
 *                           size_t             mtval);
 */
.Lhandle_error:
	/*
	 * FROM: .Lexception_might_handle
	 * FROM: .Lhandle_injected_error
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: sp
	 *
	 * Atlas:
	 *  sp: pointer to TrustedStack
	 */
	/*
	 * We're now out of the exception path, so make sure that mtdc contains
	 * the trusted stack pointer.
	 */
	cspecialw   mtdc, csp
	/*
	 * Atlas update:
	 *  mtdc: pointer to TrustedStack
	 *  sp: (still) pointer to TrustedStack
	 */

//.Lhandle_error_switcher_pcc:
	/*
	 * We want to make sure we can't leak any switcher state into error
	 * handlers, so if we're faulting in the switcher then we should force
	 * unwind.  We never change the base of PCC in the switcher, so we can
	 * check for this case by ensuring that the spilled mepcc and our current
	 * pcc have the same base.
	 */
	auipcc             ct0, 0
	clc                ct1, TrustedStack_offset_mepcc(csp)
	cgetbase           t0, ct0
	cgetbase           tp, ct1
	beq                t0, tp, .Lhandle_error_in_switcher
	// Atlas update: t1: a copy of mepcc

//.Lhandle_error_not_switcher:
	// Load the interrupted thread's stack pointer into ct0
	clc                ct0, TrustedStack_offset_csp(csp)
	// Atlas update: t0: interrupted thread's stack pointer

	/*
	 * If we have already unwound so far that the TrustedStack::frameoffset is
	 * pointing at TrustedStack::frames[0] -- that is, if the stack has no
	 * active frames on it -- then just go back to the context we came from,
	 * effectively parking this thread in a (slow) infinite loop.
	 */
	clhu               tp, TrustedStack_offset_frameoffset(csp)
	li                 t1, TrustedStack_offset_frames
	// LIVE OUT: sp
	beq                tp, t1, .Lcommon_thread_exit

	addi               tp, tp, -TrustedStackFrame_size
	cincoffset         ctp, csp, tp
	// Atlas update: tp: pointer to current TrustedStackFrame

	// a0 indicates whether we're calling a stackless error handler (0: stack,
	// 1: stackless)
	li                 a0, 0
	// Atlas update: a0: stackful (0) or stackless (1) indicator, currently 0

	// Allocate space for the register save frame on the stack.
	cincoffset         ct0, ct0, -(16*8)

//.Lhandle_error_stack_oob:
	/*
	 * WARNING: ENCODING SPECIFIC.
	 *
	 * The following depends on the fact that before-the-start values are not
	 * representable in the CHERIoT encoding and so will clear the tag.  If
	 * this property changes then this will need to be replaced by a check that
	 * against the base of the stack.  Note that this check can't be a simple
	 * cgetbase on ct0, because moving the address below the base sufficiently
	 * far that it's out of *representable* bounds will move the reported base
	 * value (base is a displacement from the address).
	 */
	cgettag            t1, ct0

	/*
	 * A value of 0xffff indicates no error handler.  Both of our conditional
	 * paths want this value, but we can load it once, now.
	 */
	li                 s1, 0xffff
	// Atlas update: s1: 0xffff

	/*
	 * If there isn't enough space on the stack, see if there's a stackless
	 * handler.
	 */
	// LIVE OUT: sp, tp, t0
	beqz               t1, .Lhandle_error_try_stackless

	clc                ct1, TrustedStackFrame_offset_calleeExportTable(ctp)
	// Atlas: t1: pointer to callee's invoked export table entry
	/*
	 * Set the export table pointer to point to the *start* of the export
	 * table.  It will currently point to the entry point that was raised.
	 *
	 * TODO: We might want to pass this to the error handler, it might be
	 * useful for providing per-entry-point error results.
	 */
	cgetbase           s0, ct1
	csetaddr           ct1, ct1, s0
	clhu               s0, ExportTable_offset_errorHandler(ct1)

//.Lhandle_error_try_stackful:
	/*
	 * A value of 0xffff indicates no error handler.  If we found one, use it,
	 * otherwise fall through and try to find a stackless handler.
	 */
	// LIVE OUT: sp, tp, t0, t1, s0, a0
	bne                s0, s1, .Lhandle_error_found

.Lhandle_error_try_stackless:
	/*
	 * FROM: above
	 * FROM: .Lhandle_error_stack_oob
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: sp, tp, s1, t0
	 * Atlas:
	 *  sp: pointer to TrustedStack
	 *  tp: pointer to current TrustedStackFrame
	 *  t0: interrupted thread's stack pointer
	 *  s1: 0xffff
	 */

	clc                ct1, TrustedStackFrame_offset_calleeExportTable(ctp)
	/*
	 * Set the export table pointer to point to the *start* of the export
	 * table.  It will currently point to the entry point that was raised.
	 */
	cgetbase           s0, ct1
	csetaddr           ct1, ct1, s0
	// Atlas: t1: pointer to callee's export table
	clhu               s0, ExportTable_offset_errorHandlerStackless(ct1)
	/*
	 * A value of 0xffff indicates no error handler.  Give up if there is no
	 * error handler for this compartment, having already tried any stackful
	 * handler.
	 */
	// LIVE OUT: mtdc
	beq                s0, s1, .Lcommon_force_unwind

	/*
	 * The stack may have had its tag cleared at this point, so for stackless
	 * handlers we need to restore the on-entry stack.
	 */
	clc                ct0, TrustedStackFrame_offset_csp(ctp)
	// Atlas: t0: target invocation's stack pointer, as of invocation start

	/*
	 * If this is the top (initial) stack frame, then the csp field is the value
	 * on entry and it is safe to use directly.  Otherwise, we reconstruct the
	 * stack as it would have been on compartment invocation.
	 */
	cincoffset         cs1, csp, TrustedStack_offset_frames
	beq                s1, tp, .Lhandle_stack_recovered

//.Lhandle_stack_rebound:
	/*
	 * The address of the stack pointer will point to the bottom of the
	 * caller's save area created by .Lswitch_entry_first_spill and following
	 * instructions, so we set the bounds to be the base up to the current
	 * address, giving the handler access to the entirety of this invocation's
	 * activation frame (except the caller save registers we spilled).
	 */
	cgetaddr           a1, ct0
	cgetbase           a2, ct0
	sub                a1, a1, a2
	csetaddr           ct0, ct0, a2
	// The code that installs the context expects the target stack to be in ct0
	csetboundsexact    ct0, ct0, a1
.Lhandle_stack_recovered:
	/*
	 * FROM: above
	 * FROM: .Lhandle_error_try_stackless
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: sp, tp, t0, t1, s0
	 *
	 * Atlas:
	 *  sp: pointer to TrustedStack
	 *  tp: pointer to current TrustedStackFrame
	 *  t0: pointer to the untrusted stack to use on invocation.  Either below
	 *      all activations, in the stackful handler case, or the entire
	 *      invocation's stack (below the spill frame created by
	 *      .Lswitch_entry_first_spill and following instructions).
	 *  t1: pointer to callee's export table
	 *  s0: offset from compartment PCC base to handler
	 */
	li                 a0, 1

.Lhandle_error_found:
	/*
	 * FROM: above
	 * FROM: .Lhandle_error_try_stackful
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: sp, tp, t0, t1, s0, a0
	 *
	 * Atlas:
	 *  sp: pointer to TrustedStack
	 *  tp: pointer to current TrustedStackFrame
	 *  t0: pointer to the untrusted stack to use on invocation.  Either below
	 *      all activations, in the stackful handler case, or the entire
	 *      invocation's stack (below the spill frame created by
	 *      .Lswitch_entry_first_spill and following instructions).
	 *  t1: pointer to callee's export table
	 *  s0: offset from compartment PCC base to handler
	 *  a0: stackful (0) or stackless (1) indicator
	 */

	// Increment the handler invocation count.
	clhu               s1, TrustedStackFrame_offset_errorHandlerCount(ctp)
	addi               s1, s1, 1
	csh                s1, TrustedStackFrame_offset_errorHandlerCount(ctp)

	/*
	 * The low bit should be 1 while we are handling a fault.  If we are in a
	 * double fault (that is, the value we just wrote back has its low bit 0),
	 * unwind now.
	 */
//.Lhandle_error_test_double_fault:
	andi               ra, s1, 1
	// LIVE OUT: mtdc
	beqz               ra, .Lcommon_force_unwind

	/*
	 * If we have reached some arbitrary limit on the number of faults in a
	 * singe compartment calls, give up now.
	 *
	 * TODO: Make this a number based on something sensible, possibly something
	 * set per entry point.  Some compartments (especially top-level ones)
	 * should be allowed to fault an unbounded number of times.
	 */
//.Lhandle_error_test_too_many:
	li                 ra, MAX_FAULTS_PER_COMPARTMENT_CALL
	// LIVE OUT: mtdc
	bgtu               s1, ra, .Lcommon_force_unwind

	// Load the pristine pcc and cgp for the invoked compartment.
	clc                cra, ExportTable_offset_pcc(ct1)
	clc                cgp, ExportTable_offset_cgp(ct1)
	/*
	 * Set the jump target to the error handler entry point.  This may result in
	 * something out-of-bounds if the compartment has a malicious value for
	 * their error handler (hopefully caught at link or load time), but if it
	 * does then we will fault when attempting the cjalr below and force unwind
	 * (either because the cjalr itself will raise a fault, because ra is
	 * untagged, or because the resulting PCC is out of bounds and instruction
	 * fetch fails; either case results in a forced unwind, albeit by slightly
	 * different paths, with .Lhandle_error_switcher_pcc relevant for the former
	 * and .Lhandle_error_test_double_fault for the latter.
	 */
	cgetbase           s1, cra
	csetaddr           cra, cra, s1
	cincoffset         cra, cra, s0

	/*
	 * If we're in an error handler with a stack, set up the stack, otherwise
	 * we just need to set up argument registers.
	 */
//.Lhandle_error_test_stackful:
	beqz               a0, .Lhandle_error_stack_setup

//.Lhandle_error_stackless_setup:
	clw                a0, TrustedStack_offset_mcause(csp)
	csrr               a1, mtval
	li                 a2, 0
	cmove              csp, ct0
	// Atlas: sp: taget compartment invocation stack pointer
	j                  .Lhandle_error_handler_invoke

.Lhandle_error_stack_setup:
	/*
	 * FROM: .Lhandle_error_test_stackful
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: ra, sp, gp, t0
	 *
	 * Atlas:
	 *  ra: handler entrypoint (with bounds of compartment's .text)
	 *  sp: pointer to TrustedStack
	 *  gp: target compartment cgp
	 *  t0: pointer to the untrusted stack to use on invocation.  This is
	 *      presently sufficiently below all activations to provide space for an
	 *      ErrorState structure.
	 */
	/*
	 * Set up the on-stack context, a compartment.h:/struct ErrorState value,
	 * which has the same layout at a TrustedStack spill frame.
	 *
	 * These begin with a PCC.  To ensure that handlers do not have access to
	 * values (especially, capabilities) reachable through the trapping PCC,
	 * we clear the tag.  Handlers of course retain access to values reachable
	 * through their own PCC and CGP.
	 */
	clc                cs1, TrustedStack_offset_mepcc(csp)
	ccleartag          cs1, cs1
	csc                cs1, TrustedStack_offset_mepcc(ct0)
	/*
	 * Now copy the 15 GPRs from the trusted stack (sp).  We use a2 as the
	 * source of the copy and a3 as the destination, preserving sp (TrustedStack
	 * pointer) and t0 (untrusted stack pointer to the base of the spill area).
	 */
	cincoffset         ca2, csp, TrustedStack_offset_cra
	cincoffset         ca3, ct0, TrustedStack_offset_cra
	copyContext        /* dst = */ ca3, /* src = */ ca2, /* scratch = */ cs1, /* counter = */ a4

	// Set up the arguments for the call
	cmove              ca0, ct0
	clw                a1, TrustedStack_offset_mcause(csp)
	csrr               a2, mtval
	cmove              csp, ca0

.Lhandle_error_handler_invoke:
	/*
	 * FROM: above
	 * FROM: .Lhandle_error_stackless_setup
	 * IRQ REQUIRE: any (see below)
	 * LIVE IN: mtdc, ra, sp, gp, a0, a1, a2
	 *
	 * Atlas:
	 *  mtdc: TrustedStack pointer
	 *  ra: handler entrypoint (with bounds of compartment's .text)
	 *  gp: target compartment cgp
	 *  sp: target compartment invocation stack pointer
	 *  a0, a1, a2: arguments to handler (see below)
	 *  tp, t0, t1, t2, s0, s1, a3, a4, a5: dead (to be zeroed)
	 */
	/*
	 * At this point, the TrustedStack spill frame is no longer precious: either
	 * we have copied it down to the untrusted stack for the stackful handler's
	 * use or we have abandoned it in deciding to use the stackless handler.
	 * Thus, our "IRQ REQUIRE: any" above: it's safe to be preemptive here,
	 * though all paths to us in fact run with IRQs deferred.
	 *
	 * Since we are not using a sentry, but rather a capability constructed from
	 * the compartment's PCC (and handler offset value) to enter the
	 * compartment, enable interrupts now.
	 */
	/*
	 * For a stackful handler, the arguments are:
	 *  - a0: equal to the invocation stack (sp), with a register spill frame
	 *        here and above (the stack grows down!)
	 *  - a1: mcause
	 *  - a2: mtval
	 *
	 * While for stackless, the arguments are:
	 *  - a0: mcause
	 *  - a1: mtval
	 *  - a2: zero
	 */
	csrsi              mstatus, 0x8
//.Lhandle_error_handler_invoke_irqs:
	// IRQ ASSUME: enabled

	// Clear all other registers and invoke the handler
	zeroAllRegistersExcept ra, sp, gp, a0, a1, a2
	cjalr              cra
//.Lhandle_error_handler_return:
	/*
	 * IFROM: above
	 * FROM: malice
	 * IRQ ASSUME: enabled (only IRQ-enabling reverse sentries given out)
	 * LIVE IN: mtdc, a0, sp
	 *
	 * Atlas:
	 *  mtdc: pointer to this thread's TrustedStack
	 *  a0: handler return value
	 *  sp: target compartment invocation stack pointer
	 *  gp, tp, t0, t1, t2, s0, s1, a1, a2, a3, a4, a5: dead (to be clobbered
	 *                                                  by replacement context
	 *                                                  or .Lcommon_force_unwind)
	 */
	/*
	 * The return sentry given to the handler as part of that cjalr could be
	 * captured in that compartment or any of its callers (recall similar
	 * commentary in switcher_after_compartment_call).  Invoking this sentry,
	 * regardless of how one comes to hold it, and even if invocation is not
	 * matched to the call that constructed any given instance of it, will
	 * always result in popping the topmost trusted stack frame (at the time of
	 * invocation) and returning to its caller.
	 *
	 * Being robust to malicious entry here is facilitated by the requirements
	 * of the next block of code being minimal: mtdc must be a TrustedStack
	 * pointer, and we may try to dereference the provided sp, but we are
	 * prepared for that to trap (and induce forced-unwinding).
	 */

	/*
	 * Now that we're back, defer interrupts again before we do anything that
	 * manipulates the TrustedStack.
	 *
	 * TODO: Eventually we'd like to move this down onto the paths where it
	 * actually matters and let most of this code run with IRQs enabled.
	 */
	csrci              mstatus, 0x8
//.Lhandle_error_handler_return_irqs:
	// IRQ ASSUME: deferred

	/*
	 * Return values are compartment.h's enum ErrorRecoveryBehaviour :
	 *  - InstallContext (0)
	 *  - ForceUnwind (1)
	 * Other values are invalid and so we should do a forced unwind anyway.
	 */
	// LIVE OUT: mtdc
	bnez               a0, .Lcommon_force_unwind

//.Lhandle_error_install_context:
	// IRQ REQUIRE: deferred (TrustedStack spill frame precious, once populated)
	/*
	 * We have been asked to install the new register context and resume.  We do
	 * this by copying the register frame over the save area and entering the
	 * exception resume path.  This may fault, but if it does then we will
	 * detect it as a double fault and forcibly unwind.
	 *
	 * The state of the target stack (sp) is expected to be common across both
	 * stackful and stackless handlers in the case of an InstallContext return.
	 * Above, in .Lhandle_error_stack_setup, we arranged for sp to point to a
	 * register spill frame (also passed in a0 for convenience from C).
	 * Stackless handlers are expected to arrange for sp to point to a register
	 * spill area before returning; compartments availing themselves of
	 * stackless handlers must also manage reserving space for such.
	 */

	cspecialr          ct1, mtdc
	// Atlas update: t1: pointer to TrustedStack
#ifdef CONFIG_MSHWM
	/*
	 * Update the spilled copy of the stack high water mark to ensure that we
	 * will clear all of the stack used by the error handler and the spilled
	 * context.
	 */
	csrr               t0, CSR_MSHWM
	csw                t0, TrustedStack_offset_mshwm(ct1)
#endif
	clhu               tp, TrustedStack_offset_frameoffset(ct1)
	addi               tp, tp, -TrustedStackFrame_size
	// Atlas update: tp: pointer to the current available trusted stack frame.
	cincoffset         ctp, ct1, tp

	/*
	 * The PCC the handler has given to us is not particularly trusted and might
	 * be an attempt to escape from the compartment.  Confine it to being
	 * derived from the compartment's (static) PCC.  This is a multi-step
	 * process, in which we...
	 *
	 * 1. Load the (tagged) PCC for the compartment, which is the 0th word in
	 *    the ExportTable.
	 */
	clc                ct0, TrustedStackFrame_offset_calleeExportTable(ctp)
	cgetbase           s0, ct0
	csetaddr           ct0, ct0, s0
	clc                ct0, ExportTable_offset_pcc(ct0)
	// Atlas update: t0: compartment .text / PCC

	// 2. Load the untrusted PCC from the handler's returned spill area (sp).
	clc                cra, TrustedStack_offset_mepcc(csp)

	/*
	 * 3. Copy the address from the returned PCC into the compartment's PCC,
	 *    which will result in an out-of-bounds capability if the handler was
	 *    trying anything fishy.
	 */
	cgetaddr           ra, cra
	csetaddr           ct2, ct0, ra
	// Atlas update: t2: program counter to resume

	/*
	 * Now copy everything else from the stack up into the trusted saved
	 * context, using a2 as the source and a3 as the destination, preserving sp
	 * (the untrusted stack pointer) and t1 (TrustedStack pointer).
	 */
	cincoffset         ca2, csp, TrustedStack_offset_cra
	cincoffset         ca3, ct1, TrustedStack_offset_cra
	copyContext        /* dst = */ ca3, /* src = */ ca2, /* scratch = */ cs1, /* counter = */ a4

	/*
	 * Increment the handler invocation count.  We have now returned and
	 * finished touching any data from the error handler that might cause a
	 * fault.  Any subsequent fault is not treated as a double fault.  It might
	 * be a fault loop, but that will be caught by the fault limit check.
	 */
	clh                s1, TrustedStackFrame_offset_errorHandlerCount(ctp)
	addi               s1, s1, 1
	csh                s1, TrustedStackFrame_offset_errorHandlerCount(ctp)

	/*
	 * Now that the context is set up, let the exception handler code deal with
	 * it.  It expects the context to be in csp, so move the context pointer
	 * there.
	 */
	cmove              csp, ct1
	// LIVE OUT: mtdc, sp, t2
	j                  .Lcommon_context_install

.Lhandle_injected_error:
	/*
	 * FROM: .Lexception_scheduler_return_installed
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: mtdc, sp
	 *
	 * Atlas:
	 *  mtdc: TrustedStack pointer
	 *  sp: TrustedStack pointer (a copy of mtdc)
	 */
#ifdef CONFIG_MSHWM
	clw                ra, TrustedStack_offset_mshwm(csp)
	csrw               CSR_MSHWM, ra
	clw                ra, TrustedStack_offset_mshwmb(csp)
	csrw               CSR_MSHWMB, ra
#endif
	j                  .Lhandle_error

.Lcommon_defer_irqs_and_thread_exit:
	/*
	 * FROM: switcher_after_compartment_call
	 * IRQ REQUIRE: any
	 */
	csrci               mstatus, 0x8
//.Lcommon_deferred_irqs_and_thread_exit:
	// IRQ ASSUME: deferred

/**
 * Signal to the scheduler that the current thread is finished
 */
.Lcommon_thread_exit:
	/*
	 * FROM: above
	 * FROM: .Lhandle_error_not_switcher
	 * IRQ REQUIRE: deferred (about to zero out MTDC and join exception path)
	 * LIVE IN: mtdc
	 *
	 * Atlas:
	 *  mtdc: pointer to TrustedStack
	 */
	csrw               mcause, MCAUSE_THREAD_EXIT
	/*
	 * mtval may have been updated by the action of other threads in the system
	 * and holds the last value latched during an exception.  From the
	 * scheduler's perspective, thread exits are a kind of exception, and
	 * exceptions get to see mtval.  Write a constant value to mtval to act more
	 * like an architectural fault and to close a small information leak to the
	 * scheduler's event handler.
	 */
	csrw               mtval, MCAUSE_THREAD_EXIT
	/*
	 * The thread exit code expects the TrustedStack pointer to be in csp and
	 * the thread's stack pointer to be in mtdc.  After thread exit, we don't
	 * need the stack pointer so just put zero there.
	 */
	zeroOne            sp
	cspecialrw         csp, mtdc, csp
	// LIVE OUT: mtdc, sp
	j                  .Lexception_exiting_threads_rejoin

	/*
	 * Some switcher instructions' traps are handled specially, by looking at
	 * the offset of mepcc.  Otherwise, we're off to a force unwind.
	 */
.Lhandle_error_in_switcher:
	/*
	 * FROM: .Lhandle_error_switcher_pcc
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: mtdc, t1
	 *
	 * Atlas:
	 *  mtdc:  pointer to TrustedStack
	 *  t1: A copy of mepcc, the faulting program counter
	 */
	auipcc             ctp, %cheriot_compartment_hi(.Lswitch_entry_first_spill)
	cincoffset         ctp, ctp, %cheriot_compartment_lo_i(.Lhandle_error_in_switcher)
	bne                t1, tp, .Lcommon_force_unwind
	li                 a0, -ENOTENOUGHSTACK
	li                 a1, 0

	/*
	 * Cause the interrupted thread to resume as if a return had just executed.
	 * We do this by vectoring to a `cjalr ra` (`cret`) instruction through
	 * `mepcc`; whee!  Overwrites the stored context a0 and a1 with the current
	 * values of those registers, effectively passing them through
	 * .Lcommon_context_install.
	 */
.Lhandle_return_context_install:
	/*
	 * FROM: above
	 * IRQ REQUIRE: deferred (TrustedStack spill frame is precious)
	 * LIVE IN: sp, a0, a1
	 *
	 * Atlas:
	 *  sp: pointer to TrustedStack
	 *  a0, a1: return values to the caller
	 */
	auipcc             ct2, %cheriot_compartment_hi(.Lswitch_just_return)
	cincoffset         ct2, ct2, %cheriot_compartment_lo_i(.Lhandle_return_context_install)
	csc                ca0, TrustedStack_offset_ca0(csp)
	csc                ca1, TrustedStack_offset_ca1(csp)
	// LIVE OUT: sp, t2
	j                  .Lcommon_context_install

.Lexception_reentered:
	/*
	 * FROM: exception_entry_asm
	 * FROM: .Lexception_reentered
	 * IRQ REQUIRE: deferred (an IRQ before we reprogram MTCC could escape
	 *              looping)
	 */
	/*
	 * We've reentered our exception handler, a "double fault" of sorts.  Make
	 * sure that we end up in an architectural trap loop: clobber mtcc, so that
	 * that trap attempts to vector to an untagged PCC, thereby causing another
	 * trap, which immediately traps, and so on.
	 *
	 * We could instead zero mtdc, ensuring that we spin through several
	 * instructions (taking a trap then running enough of exception_entry_asm
	 * until we again trapped), but this is less architecturally visible.
	 */
	/*
	 * Writing cnull to mtcc takes two instructions because cspecialw is an
	 * alias for cspecialrw with a zero source, which means "don't write".  So,
	 * put nullptr in a register with non-zero index, and then put that in mtcc.
	 */
	zeroOne            sp
	cspecialw          mtcc, csp
	// Take a trap and wedge the machine on that null MTCC
	clc                csp, 0(csp)
	j                  .Lexception_reentered

.size exception_entry_asm, . - exception_entry_asm

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
	/*
	 * FROM: malice
	 * IRQ ASSUME: deferred
	 * LIVE IN: mtdc, callee-save, ra, a0
	 *
	 * Atlas:
	 *  mtdc: pointer to TrustedStack (or nullptr if from buggy scheduler)
	 *  ra: return pointer (guaranteed because this symbol is reachable only
	 *      through an interrupt-disabling forward-arc sentry)
	 *  a0: requested number of trusted stack frames
	 */
	li                 a2, TrustedStackFrame_size
	mul                a2, a0, a2
	// Atlas update: a2: requested number trusted stack frames, in bytes
	/*
	 * Load the trusted stack into the return register, so that we clobber it on
	 * the way out.  Nothing here should trap, but if it does we'll forcibly
	 * unwind (see .Lhandle_error_in_switcher) and also clobber this pointer.
	 */
	cspecialr          ca0, mtdc
	/*
	 * TrustedStack::frames[] is a FAM at the end of the structure, and
	 * ::frameoffset codes for our current position therein (by counting bytes
	 * relative to the start of the TrustedStack).  We have sufficiently many
	 * frames if the TrustedStack length minus ::frameoffset is greater than
	 * the requested number of bytes.
	 */
	clhu               a1, TrustedStack_offset_frameoffset(ca0)
	// Atlas update: a1: this thread's TrustedStack::frameoffset
	cgetlen            a0, ca0
	// Atlas update: a0: length of this thread's TrustedStack
	sub                a0, a0, a1
	sltu               a0, a2, a0
	// LIVE OUT: mtdc, a0
	cret

// Reveal the stack pointer given to this compartment invocation
	.section .text, "ax", @progbits
	.p2align 2
	.type __Z22switcher_recover_stackv,@function
__Z22switcher_recover_stackv:
	/*
	 * FROM: malice
	 * IRQ ASSUME: deferred
	 * LIVE IN: mtdc, callee-save, ra
	 *
	 * Atlas:
	 *  mtdc: pointer to TrustedStack (or nullptr if buggy scheduler)
	 *  ra: return pointer (guaranteed because this symbol is reachable only
	 *      through an interrupt-disabling forward-arc sentry)
	 */
	/*
	 * Load the trusted stack pointer into a register that we will clobber after
	 * two instructions.
	 */
	cspecialr          ca0, mtdc
	// Atlas update: a0: pointer to TrustedStack
	clhu               a1, TrustedStack_offset_frameoffset(ca0)
	// Atlas update: a1: TrustedStack::frameoffset
	addi               a1, a1, -TrustedStackFrame_size
	// Atlas update: a1: offset of current TrustedStackFrame
	cincoffset         ca0, ca0, a1
	// Atlas update: a0: pointer to current TrustedStackFrame
	clc                ca0, TrustedStackFrame_offset_csp(ca0)
	// Atlas update: a0: saved stack pointer at time of frame creation
	/*
	 * If this is the first frame, then the recovered stack will be the stack
	 * on entry, and can be returned directly.
	 */
	li                 a2, TrustedStack_offset_frames
	// Atlas update: a2: dead but exposed: TrustedStack_offset_frames
	beq                a1, a2, 0f

	/*
	 * Otherwise, this is not the first frame, and the TrustedStackFrame::csp
	 * value is pointing to the spills done at .Lswitch_entry_first_spill.  Redo
	 * the stack chopping done at .Lswitch_stack_chop to recompute the bounds
	 * we would have given to the callee.
	 */
	cgetaddr           a1, ca0
	cgetbase           a2, ca0
	sub                a1, a1, a2
	csetaddr           ca0, ca0, a2
	csetboundsexact    ca0, ca0, a1
	/*
	 * Atlas update:
	 *  a1: dead but exposed: the length of the stack
	 *  a2: dead but exposed: base address of the stack
	 */
0:
	// LIVE OUT: mtdc, a0
	cret

	.section .text, "ax", @progbits
	.p2align 2
	.type __Z30trusted_stack_interrupt_threadPv,@function
__Z25switcher_interrupt_threadPv:
	/*
	 * FROM: malice
	 * IRQ ASSUME: deferred
	 * LIVE IN: mtdc, callee-save, ra, a0
	 *
	 * Atlas:
	 *   mtdc: pointer to TrustedStack (or nullptr if buggy scheduler)
	 *   a0: sealed pointer to target thread TrustedStack
	 *   ra: return pointer (guaranteed because this symbol is reachable only
	 *       through an interrupt-disabling forward-arc sentry)
	 */
	/*
	 * Because this function involves looking across two threads' states, it
	 * needs to run with preemption prohibited, and that means IRQs deferred.
	 */

	// Load the unsealing key
	LoadCapPCC         ca1, .Lsealing_key_trusted_stacks
	/*
	 * The target capability is in ca0.  Unseal, clobbering our authority;
	 * check tag; and load the entry point offset.
	 */
	cunseal            ca1, ca0, ca1
	// Atlas update: a1: unsealed pointer to target thread TrustedStack
	/*
	 * LOCAL SEAL: Nothing herein depends on a1 being GL(obal).
	 */
	cgettag            a0, ca1
	// a0 (return register) now contains the tag.  We return false on failure
	// so can just branch to the place where we zero non-return registers from
	// here and it will contain faluse on failure.
	beqz               a0, .Lswitcher_interrupt_thread_return

	// A thread can't interrupt itself, return failure if it tries.
	cspecialr          ca2, mtdc
	li                 a0, 0
	beq                a2, a1, .Lswitcher_interrupt_thread_return
	// Atlas update: a2: unsealed pointer to current thread TrustedStack

	/*
	 * We allow the target thread to be interrupted if (and only if) the caller
	 * is in the same compartment as the interrupted thread.  We will determine
	 * this by checking if the base of the two export table entries from the
	 * top of the trusted stack frames match.
	 */

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
	bne                a2, a3, .Lswitcher_interrupt_thread_return
	// Atlas update: a1, a2, a3: dead (to be zeroed)

	/*
	 * Mark the thread as interrupted.  Store a magic value in mcause.  This
	 * value will not be overwritten by a trap before the scheduler sees the
	 * target thread, since we are on core and it isn't.
	 */
	li                 a2, MCAUSE_THREAD_INTERRUPT
	csw                a2, TrustedStack_offset_mcause(ca1)
	// Return success
	li                 a0, 1
.Lswitcher_interrupt_thread_return:
	zeroRegisters      a1, a2, a3
	cret

// Get a sealed pointer to the current thread's TrustedStack
	.section .text, "ax", @progbits
	.p2align 2
	.type __Z23switcher_current_threadv,@function
__Z23switcher_current_threadv:
	/*
	 * FROM: malice
	 * IRQ ASSUME: deferred
	 * LIVE IN: mtdc, callee-save, ra
	 *
	 * Atlas:
	 *   mtdc: pointer to TrustedStack (or nullptr if buggy scheduler)
	 *   ra: return pointer (guaranteed because this symbol is reachable only
	 *       through an interrupt-disabling forward-arc sentry)
	 */

	LoadCapPCC         ca0, .Lsealing_key_trusted_stacks
	// Atlas update: a0: sealing authority for trusted stacks
	cspecialr          ca1, mtdc
	// Atlas update: a1: copy of mtdc
	cseal              ca0, ca1, ca0
	li                 a1, 0
	/*
	 * Atlas update:
	 *   a0: sealed copy of mtdc, this thread's TrustedStack
	 *   a1: zero
	 */
	cret

// Get a pointer to this thread's hazard pointers array
	.section .text, "ax", @progbits
	.p2align 2
	.type __Z28switcher_thread_hazard_slotsv,@function
__Z28switcher_thread_hazard_slotsv:
	/*
	 * FROM: malice
	 * IRQ ASSUME: deferred
	 * LIVE IN: mtdc, callee-save, ra
	 *
	 * Atlas:
	 *   mtdc: pointer to TrustedStack (or nullptr if buggy scheduler)
	 *   ra: return pointer (guaranteed because this symbol is reachable only
	 *       through an interrupt-disabling forward-arc sentry)
	 */

	cspecialr          ca0, mtdc

	// If this traps (from null mtdc, say), we'll forcibly unwind.
	clc                ca0, TrustedStack_offset_hazardPointers(ca0)
	// Atlas update: a0: pointer to hazard pointers

	cret

// Get the current thread's integer ID
	.section .text, "ax", @progbits
	.p2align 2
	.type __Z13thread_id_getv,@function
__Z13thread_id_getv:
	/*
	 * FROM: malice
	 * IRQ ASSUME: deferred
	 * LIVE IN: mtdc, callee-save, ra
	 *
	 * Atlas:
	 *   mtdc: pointer to TrustedStack (or nullptr if buggy scheduler)
	 *   ra: return pointer (guaranteed because this symbol is reachable only
	 *       through an interrupt-disabling forward-arc sentry)
	 */

	cspecialr          ca0, mtdc
	/*
	 * If this is a null pointer, don't try to dereference it and report that
	 * we are thread 0.  This permits the debug code to work even from things
	 * that are not real threads.
	 */
	cgettag            a1, ca0
	// Atlas update: a1: tag of a0/mtdc
	beqz               a1, 0f
	clh                a0, TrustedStack_offset_threadID(ca0)
	// Atlas update: a0: integer ID of current thread
0:
	cret


// Return the stack high-water mark
	.section .text, "ax", @progbits
	.p2align 2
	.type __Z25stack_lowest_used_addressv,@function
__Z25stack_lowest_used_addressv:
	csrr               a0, CSR_MSHWM
	cret

// Reset the count of error handler invocations in this compartment invocation
	.section .text, "ax", @progbits
	.p2align 2
	.type __Z39switcher_handler_invocation_count_resetv,@function
__Z39switcher_handler_invocation_count_resetv:
	/*
	 * FROM: malice
	 * IRQ ASSUME: deferred
	 * LIVE IN: mtdc, callee-save, ra
	 *
	 * Atlas:
	 *   mtdc: pointer to TrustedStack (or nullptr if buggy scheduler)
	 *   ra: return pointer (guaranteed because this symbol is reachable only
	 *       through an interrupt-disabling forward-arc sentry)
	 */

	cspecialr          ca1, mtdc
	// Atlas update: a1: copy of mtdc
	clhu               a0, TrustedStack_offset_frameoffset(ca1)
	addi               a0, a0, -TrustedStackFrame_size
	// Atlas update: a0: offset of the current trusted stack frame
	cincoffset         ca1, ca1, a0
	/*
	 * Atlas update:
	 *  a0: dead
	 *  a1: pointer to current TrustedStack::frame
	 */
	clh                a0, TrustedStackFrame_offset_errorHandlerCount(ca1)
	// Atlas update: a0: current invocation count (for return)
	// Reset invocation count
	csh                zero, TrustedStackFrame_offset_errorHandlerCount(ca1)
	// Atlas update: a1: dead (to be zeroed)
	li                 a1, 0
	cret

// The linker expects export tables to start with space for cgp and pcc, then
// the compartment error handler.  We should eventually remove that requirement
// for library export tables, but since they don't consume RAM after loading
// it's not urgent.
	.section	.compartment_export_table,"a",@progbits
.p2align	3
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
/*
 * We mangle the switcher export as if it were a compartment call, but see
 * loader/boot.cc's special handling of this entry.
 */
export __Z26compartment_switcher_entryz, __export_switcher
export __Z23trusted_stack_has_spacei
export __Z22switcher_recover_stackv
export __Z25switcher_interrupt_threadPv
export __Z23switcher_current_threadv
export __Z28switcher_thread_hazard_slotsv
export __Z13thread_id_getv
export __Z25stack_lowest_used_addressv
export __Z39switcher_handler_invocation_count_resetv

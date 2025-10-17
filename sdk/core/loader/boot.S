// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "constants.h"
#include "defines.h"
#include "../switcher/trusted-stack-assembly.h"
#include <cheri-builtins.h>

.include "assembly-helpers.s"

    .section .loader_start, "ax", @progbits

/*
 * Platforms may wish to hook the boot code before we've said anything at all.
 *
 * The "interface" provided to this #include is that the effects of the above
 * #include-s are in scope, as are the local symbols of this file, and that we
 * have introduced the ".loader_start" linker .section, which is presumed not to
 * have contents defined in any other file and is placed by the
 * sdk/firmware.rocode.ldscript.in file immediately after the well-known
 * "_start" symbol.
 *
 * This #include is therefore in a good position to insert very early
 * instructions if necessary and/or to define other .sections related to the
 * boot process.  It could even move this loader to a different .section.
 */
#if __has_include(<platform-early_boot_prestart.inc>)
#       include <platform-early_boot_prestart.inc>
#endif

    .p2align 2
    .type .Lstart,@function
.Lstart:

	/*
	 * Any code between here and the call into the scheduler entry point (as
	 * returned by the loader), including the loader itself, has access to the
	 * root capabilities.
	 */

	// Load and keep the RW memory root in ca4 throughout
	cspecialr		ca4, mtdc

	// Build the RW root without GL(obal) in a5
	li				a5, ~CHERI_PERM_GLOBAL
	candperm		ca5, ca4, a5

	// Prepare a C execution stack
	la_abs			a3, bootStack
	li				a1, BOOT_STACK_SIZE
	csetaddr		csp, ca5, a3
	csetboundsexact	csp, csp, a1
	cincoffset		csp, csp, a1 // Move to the end and grow downwards.

	/*
	 * Platforms may wish to hook the boot code after we've gotten into "start".
	 * This is the right place to fill in any essential architectural features
	 * whose on-reset values are unspecified (likely to simplify gateware).
	 *
	 * This #include is expected to generate instructions, but of course other
	 * fancy stunts are possible (such as defining .subsections of .loader_start
	 * for new symbols).
	 *
	 * At this point, the following register allocations are in place and are
	 * expected to be preserved by any code emitted by this #include:
	 *
	 *  - sp/x2 : holds the stack pointer with which the loader will run; this
	 *            can be used as scratch space by the emitted code and need not
	 *            be zeroed upon exit.
	 *
	 *  - a4/x14: holds the RW cap root.
	 *
	 *  - a5/x15: holds the RW cap root without GL(obal) permission.
	 *
	 * In particular, ra/x1, a0/x10, and a1/x11 are free and may be clobbered,
	 * as platform code is quite likely to want to use .Lfill_block, below.
	 */
#if __has_include(<platform-early_boot.inc>)
#	include <platform-early_boot.inc>
#endif

	// Prepare a bounded, data RO, local pointer to the header.
	la_abs			a1, __compart_headers
	la_abs			a3, __compart_headers_end
	sub				a3, a3, a1
	li				a2, ~(CHERI_PERM_STORE | \
					      CHERI_PERM_LOAD_STORE_CAP | \
					      CHERI_PERM_GLOBAL)
	candperm		ca2, ca4, a2
	csetaddr		ca1, ca2, a1
	// FIXME: This should be a set bounds exact, but we currently don't have a
	// 'pad to capability alignment' command in the linker script and it needs
	// to span two sections.
	csetbounds		ca1, ca1, a3

	// We just want to grab the EXE root. Offset in auipcc matters not.
	auipcc			ca2, 0

	/*
	 * Set up ra to be the loader's C++ entry point.
	 *
	 * We are safe to clobber ra here because this is the root function on
	 * the call stack.
	 */
		// First set the lower bound on the loader's PCC:
	la_abs			s0, .loader_code_start
	csetaddr		cra, ca2, s0
		// Set the size
	la_abs			s0, .loader_code_size
	csetboundsexact	cra, cra, s0
		// Set the C++ entry point of loader
	la_abs			s0, loader_entry_point
	csetaddr		cra, cra, s0

	/*
	 * Build the loader's globals pointer.  The base address and length are
	 * specified in the header, and the permissions are *stack-flavored* (that
	 * is, granting S(tore)L(ocal) and not GL(obal).
	 */
	la_abs			s0, .loader_data_start
	csetaddr		cgp, ca5, s0 // ca5 has the SL root.
	la_abs			s0, .loader_data_size
	csetboundsexact	cgp, cgp, s0
	srli			s0, s0, 1
	cincoffset		cgp, cgp, s0

	/*
	 * mscratchc still has the sealing root; take it and stash the RW root.
	 * This is one instruction faster below than leaving the only copy in mtdc.
	 */
	cspecialrw		ca3, mscratchc, ca4

	// ca4 still has the RW memory root; nothing to change

	/*
	 * The first argument (ca0) is a SchedEntryInfo out parameter.  Make space
	 * for it at the base of the stack, where we can easily find it again after
	 * the call returns.
	 */
	la_abs			s1, __thread_count
	csetaddr		cs1, ca4, s1
	clhu			s1, 0(cs1)
	li			t0, -BOOT_THREADINFO_SZ
	mul			s1, s1, t0
	addi			s1, s1, -SchedulerEntryInfo_offset_threads
	cincoffset		csp, csp, s1
	neg			s1, s1
	csetbounds		ca0, csp, s1

	// Call to loader_entry_point.
	cjalr			cra

	/*
	 * Load the two return values (pcc and cgp for the scheduler entry point).
	 *
	 * Drop these elements from the on-stack SchedulerEntryInfo, leaving only
	 * its array of ThreadInfo-s (pointed to by sp).
	 */
	clc				cs0, 0(csp)
	clc				cgp, 8(csp)
	cincoffset		csp, csp, SchedulerEntryInfo_offset_threads

	// Zero the stack (except for the aforementioned threadInfo-s).
	cgetbase		a0, csp
	csetaddr		ca0, csp, a0
	cmove			ca1, csp
	cjal			.Lfill_block
	// Nothing in the loader stores to the stack after this point

	/*
	 * Clear most capability roots.
	 *
	 * - mtcc (aka mtvecc, mtvec) has been set by the loader_entry_point.
	 * - mtdc is going to be set below.
	 * - mscratchc holds a copy of mtdc, the RW (G+SL) root, which we briefly
	 *   want again, so take it while writing zero
	 * - mepcc is safe to zero
	 */
	zeroOne			a0
	cspecialw		mepcc, ca0
	cspecialrw		ca0, mscratchc, ca0

	// Zero the entire heap
	la_abs			a1, __export_mem_heap
	csetaddr		ca0, ca0, a1
	la_abs			a1, __export_mem_heap_end
	cjal			.Lfill_block

	/*
	 * Prepare a trusted stack for the idle thread.  This structure is almost
	 * entirely present to simplify the scheduler.  Unlike other trusted stacks
	 * in the system, it does not make reference to a compartment export table.
	 *
	 * This clobbers mtdc's initial value of the RW root.
	 */
	la_abs			a3, bootTStack
	li				a1, BOOT_TSTACK_SIZE
	csetaddr		ca3, ca0, a3 // ca0 still has the RW (G+SL) root
	csetboundsexact	ca3, ca3, a1
	li				a1, TSTACKOFFSET_FIRSTFRAME
	csh				a1, TrustedStack_offset_frameoffset(ca3)
	cspecialw		mtdc, ca3

	// Move the scheduler's (bounded) PCC into the register we'll jump to later.
	cmove			cra, cs0

	/*
	 * Pass, as the first argument, the array of ThreadInfo-s from what was the
	 * on-stack SchedulerEntryInfo.
	 *
	 * This clobbers a0's copy of the RW root; all roots are now gone.
	 */
	addi			s1, s1, -SchedulerEntryInfo_offset_threads
	csetbounds		ca0, csp, s1

	/*
	 * The machine is now about to approximate the behavor of the idle thread
	 * (mtdc, sp) making a compartment call into the scheduler compartment (ra
	 * and gp) and in particular its initialization hook, scheduler_entry.
	 * This is not a proper cross-call, especially in that neither the call nor
	 * return will go via the switcher's cross-call path (or use the trusted
	 * stack), because we are not a fully-fledged RTOS thread.
	 *
	 * Clear all other registers before invoking.  This ensures that the
	 * scheduler does not, for example, get access to capability roots.
	 */
	zeroAllRegistersExcept	ra, sp, gp, a0
	cjalr			cra

	/*
	 * Done scheduler setup.  Now prepare to be the idle thread.
	 *
	 * Drop our reference to the stack, as it is now exclusively property of the
	 * scheduler, having been installed over in switcher/entry.S's
	 * switcher_scheduler_entry_csp.
	 */
	zeroOne			sp

	// Enable external and timer interrupts.
	li				t1, (MIE_MEIE | MIE_MTIE)
	csrs			mie, t1
	// Globally enable interrupts.
	csrsi			mstatus, MSTATUS_MIE

	/*
	 * Yield to the scheduler to start real tasks, if a well-timed interrupt
	 * hasn't already kicked off the machinery.  We are enough like a real RTOS
	 * thread that we can do this just like stdlib.h's yield().
	 */
	ecall

	// The idle thread sleeps and only waits for interrupts.
.Lidle_loop:
	wfi
	j				.Lidle_loop

.Lfill_block:
	csc				c0, 0(ca0)
	cincoffset		ca0, ca0, 8
	bltu			a0, a1, .Lfill_block
	cret
.size start, . - start

	.section .loader_trusted_stack, "a", @progbits
	.type bootTStack, @object
	.size bootTStack, BOOT_TSTACK_SIZE
bootTStack:
	.fill BOOT_TSTACK_SIZE

	.section .loader_stack, "a", @progbits
	.type bootStack, @object
	.size bootStack, BOOT_STACK_SIZE
bootStack:
	.fill BOOT_STACK_SIZE

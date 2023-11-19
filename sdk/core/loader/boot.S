// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "constants.h"
#include "defines.h"
#include "../switcher/trusted-stack-assembly.h"
#include <cheri-builtins.h>

.include "assembly-helpers.s"

    .section .loader_start, "ax", @progbits
    .globl start
    .p2align 2
    .type start,@function
start:
	cjal			.Lregs_clear
	// The common register clearing function will not zero these registers.
	zeroRegisters	ra, sp, gp, a0
	// At this point all registers are cleared.
#if __has_include(<platform-early_boot.inc>)
#	include <platform-early_boot.inc>
#endif

	la_abs			a3, bootStack
	li				a1, BOOT_STACK_SIZE
	cspecialr		ca4, mtdc // Keep the RW memory root in ca4 throughout
	li				a2, ~CHERI_PERM_STORE_LOCAL
	li				a5, ~CHERI_PERM_GLOBAL
	// Keep G in ca2 and SL in ca5.
	candperm		ca2, ca4, a2
	candperm		ca5, ca4, a5
	csetaddr		csp, ca5, a3
	csetboundsexact	csp, csp, a1
	cincoffset		csp, csp, a1 // Move to the end and grow downwards.

	// Prepare a trusted stack for the loader.
	la_abs			a3, bootTStack
	li				a1, BOOT_TSTACK_SIZE
	csetaddr		ca3, ca4, a3 // ca4 still has the RW (G+SL!) root
	csetboundsexact	ca3, ca3, a1
	li				a1, TSTACKOFFSET_FIRSTFRAME
	csh				a1, TrustedStack_offset_frameoffset(ca3)
	cspecialw		mtdc, ca3

	// Prepare a bounded pointer to the header.
	la_abs			a1, __compart_headers
	la_abs			a3, __compart_headers_end
	sub				a3, a3, a1
	csetaddr		ca1, ca2, a1 // ca2 still has the G root.
	// FIXME: This should be a set bounds exact, but we currently don't have a
	// 'pad to capability alignment' command in the linker script and it needs
	// to span two sections.
	csetbounds		ca1, ca1, a3
	// Set up $cra to be the loader's C++ entry point.
	// We are safe to clobber $cra here because this is the root function on
	// the call stack.
	// First set the lower bound on the loader's PCC:
	auipcc			cra, 0
	clw				s0, IMAGE_HEADER_LOADER_CODE_START_OFFSET(ca1)
	csetaddr		cra, cra, s0
	// Set the size
	clhu			s0, IMAGE_HEADER_LOADER_CODE_SIZE_OFFSET(ca1)
	csetboundsexact	cra, cra, s0
	// Set the C++ entry point of loader
	la_abs			s0, loader_entry_point
	csetaddr		cra, cra, s0
	// Base and size of the GP of loader
	// Flute doesn't support unaligned loads, so we have to load the base as
	// bytes
	clbu			s0, IMAGE_HEADER_LOADER_DATA_START_OFFSET+3(ca1)
	sll				s0, s0, 8
	clbu			s1, IMAGE_HEADER_LOADER_DATA_START_OFFSET+2(ca1)
	add				s0, s0, s1
	sll				s0, s0, 8
	clbu			s1, IMAGE_HEADER_LOADER_DATA_START_OFFSET+1(ca1)
	add				s0, s0, s1
	sll				s0, s0, 8
	clbu			s1, IMAGE_HEADER_LOADER_DATA_START_OFFSET+0(ca1)
	add				s0, s0, s1
	clhu			s1, IMAGE_HEADER_LOADER_DATA_SIZE_OFFSET(ca1)
	csetaddr		cgp, ca5, s0 // ca5 has the SL root.
	csetboundsexact	cgp, cgp, s1
	srli			s1, s1, 1
	cincoffset		cgp, cgp, s1

	// We just want to grab the EXE root. Offset in auipcc matters not.
	auipcc			ca2, 0
	cgetbase		t1, ca2
	csetaddr		ca2, ca2, t1
	// mscratchc still has the sealing root; take it and stash the RW root
	cmove			ca3, ca4
	cspecialrw		ca3, mscratchc, ca3
	// ca4 still has the RW memory root; nothing to change
	// The return value is SchedEntryInfo, the space for it is in ca0
	// See the comment at the start of `loader_entry_point` in `boot.cc`.

	la_abs			s1, __thread_count
	csetaddr		cs1, ca4, s1
	clhu			s1, 0(cs1)
	li			t0, -BOOT_THREADINFO_SZ
	mul			s1, s1, t0
	addi			s1, s1, -SchedulerEntryInfo_offset_threads

	cincoffset		csp, csp, s1
	neg			s1, s1
	csetbounds		ca0, csp, s1

	// Jump to loader_entry_point.
	cjalr			cra

	// Load the two return values (pcc and cgp for the scheduler entry point)
	clc				cs0, 0(csp)
	clc				cgp, 8(csp)
	cincoffset		csp, csp, SchedulerEntryInfo_offset_threads

	// Reset the stack pointer to point to the top and clear it
	cgetbase		a0, csp
	csetaddr		ca0, csp, a0
	cmove			ca1, csp
	cjal			.Lfill_block
	// Nothing in the loader stores to the stack after this point

	// Zero the entire heap and clear roots.
	cspecialr		ca0, mscratchc // RW root temporarily held here
	la_abs			a1, __export_mem_heap
	csetaddr		ca0, ca0, a1
	la_abs			a1, __export_mem_heap_end
	cjal			.Lfill_block
	// Clear the remaining roots.
	// mtdc is serving its purpose since being set above, and mtcc
	// has been set by the loader_entry_point.
	zeroOne			a0
	cspecialw		mepcc, ca0
	cspecialw		mscratchc, ca0

	// Move the scheduler's PCC into the register we'll jump to later.
	cmove			cra, cs0
	// c1 is cra (new PCC), c2 is csp, c3 is cgp.
	// All other registers will be cleared in the clear-regs block

	// Pass the array of threadInfos as first argument.
	addi			s1, s1, -16
	csetbounds		ca0, csp, s1

.Lregs_clear:
	// a0 is used to pass arguments to the scheduler entry.
	zeroAllRegistersExcept	ra, sp, gp, a0
	cjalr			cra

	// Done scheduler setup. Now prepare an idle thread.
	zeroOne			sp
	li				t1, 0x880
	// Enable external and timer interrupts.
	csrs			mie, t1
	// Globally enable interrupts.
	csrsi			mstatus, 0x8
	// Yield to the scheduler to start real tasks.
	ecall
	// The idle thread sleeps and only waits for interrupts.
.Lidle_loop:
	// There is a bug in the Flute hardware revoker that means it stops during
	// wfi, but we want it to run here.  Flute is simulation only at the
	// moment, so we don't care that the nop is power-inefficient.
#if defined(TEMPORAL_SAFETY) && defined(FLUTE) && !defined(SOFTWARE_REVOKER)
	nop
#else
	wfi
#endif
	j				.Lidle_loop

.Lfill_block:
	csc				c0, 0(ca0)
	cincoffset		ca0, ca0, 8
	bltu			a0, a1, .Lfill_block
	cret
.size start, . - start

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
	// The common register clearing function will not zero these registers.
	zeroRegisters	ra, sp, gp, a0
	cjal			.Lregs_clear
	// At this point all registers are cleared.
#ifdef FLUTE
	// Ugly hack. For some reason if I don't give this first load, subsequent mem
	// ops will trap on Flute-TCM.
	li				a3, 0x80000000
	cspecialr		ca4, mtdc
	csetaddr		ca4, ca4, a3
	clc				c0, 0(ca4)
	li				a0, SHADOW_BASE
	csetaddr		ca0, ca4, a0
	li				a1, SHADOW_BASE + SHADOW_SZ
	cjal			.Lfill_block
#endif

	la_abs			a3, bootStack
	li				a1, BOOT_STACK_SIZE
	cspecialr		ca2, mtdc
	li				a4, ~CHERI_PERM_STORE_LOCAL
	li				a5, ~CHERI_PERM_GLOBAL
	// Keep G in ca4 and SL in ca5.
	candperm		ca4, ca2, a4
	candperm		ca5, ca2, a5
	csetaddr		csp, ca5, a3
	csetboundsexact	csp, csp, a1
	cincoffset		csp, csp, a1 // Move to the end and grow downwards.

	// Prepare a trusted stack for the loader.
	la_abs			a3, bootTStack
	li				a1, BOOT_TSTACK_SIZE
	csetaddr        ca3, ca2, a3 // ca2 has the RW root
	csetboundsexact	ca3, ca3, a1
	li				a1, TSTACKOFFSET_FIRSTFRAME
	csh				a1, TrustedStack_offset_frameoffset(ca3)
	cspecialrw		ctp, mscratchc, ca3

	// Prepare a bounded pointer to the header.
	la_abs			a2, __compart_headers
	la_abs			a3, __compart_headers_end
	sub				a3, a3, a2
	csetaddr		ca1, ca4, a2 // ca4 has the G root.
	csetboundsexact	ca1, ca1, a3
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
	auipcc          ca2, 0
	cgetbase		t1, ca2
	csetaddr		ca2, ca2, t1
	// ctp still has the sealing root.
	cmove			ca3, ctp
	// mtdc still has the memory root.
	cspecialr		ca4, mtdc
	// The return value is SchedEntryInfo.
	cincoffset		csp, csp, -16 - CONFIG_THREADS_NUM * BOOT_THREADINFO_SZ
	csetbounds		ca0, csp, 16 + CONFIG_THREADS_NUM * BOOT_THREADINFO_SZ

	// Jump to loader_entry_point.
	cjalr			cra

	// Load the two return values (pcc and cgp for the scheduler entry point)
	clc				cs0, 0(csp)
	clc				cgp, 8(csp)
	cincoffset		csp, csp, 16

	// Reset the stack pointer to point to the top and clear it
	cgetbase		a0, csp
	csetaddr		ca0, csp, a0
	cmove			ca1, csp
	cjal			.Lfill_block
	// Nothing in the loader stores to the stack after this point

	// Zero the entire heap and clear roots.
	cspecialr		ca0, mtdc // RW root is still in MTDC.
	la_abs			a1, __export_mem_heap
	csetaddr		ca0, ca0, a1
	la_abs			a1, __export_mem_heap_end
	cjal			.Lfill_block
	// Clear the roots.
	zeroOne			a0
	cspecialw		mepcc, ca0
	cspecialw		mtdc, ca0

	// Move the scheduler's PCC into the register we'll jump to later.
	cmove			cra, cs0
	// c1 is cra (new PCC), c2 is csp, c3 is cgp.
	// All other registers will be cleared in the clear-regs block

	// Pass the array of threadInfos as first argument.
	csetbounds		ca0, csp, CONFIG_THREADS_NUM * BOOT_THREADINFO_SZ

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
	wfi
	j				.Lidle_loop

.Lfill_block:
	csc				c0, 0(ca0)
	cincoffset		ca0, ca0, 8
	bltu			a0, a1, .Lfill_block
	cret
.size start, . - start

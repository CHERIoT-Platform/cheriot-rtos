// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/// Load the absolute address of a symbol.
.macro la_abs reg, symbol
    lui             \reg, %hi(\symbol)
    addi            \reg, \reg, %lo(\symbol)
.endm

/**
 * Helper macro for zeroing a single register.
 * Use c.li to guarantee it's 2 bytes and in the base ISA.
 */
.macro zeroOne reg
	c.li \reg, 0
.endm

/**
 * Helper macro for applying a macro to each argument in a list.  Calls `m`
 * once for each subsequent argument.
 */
.macro forall m rhead rtail:vararg
	\m \rhead
	.ifnb \rtail
	forall \m \rtail
	.endif
.endm

/// Zero all of the registers in a list
.macro zeroRegisters reg1, regs:vararg
	forall zeroOne, \reg1, \regs
.endm

/**
 * Helper that sets a symbol based on a register name to 1.  Used for macros
 * that operate over all registers *except* a set.
 */
.macro setValueForRegName reg
	.set __use_reg\reg, 1
.endm

/**
 * Helper that sets a symbol based on a register name to 0.  Used for macros
 * that operate over all registers *except* a set.
 */
.macro clearValueForRegName reg
	.set __use_reg\reg, 0
.endm

/**
 * Zeroes a register if it is in a set that has had `setValueForRegName` called
 * with the same counter and regname values.
 */
.macro zeroIfIncluded reg
	.ifgt __use_reg\reg
		zeroOne \reg
	.endif
.endm

/**
 * Zero all registers *except* those provided as arguments.
 * Note that this macro defines a bunch of __use_reg symbols to make it work.
 */
.macro zeroAllRegistersExcept regs:vararg
	// XXX: There must be a way to not write the list twice in pure assembly.
	forall setValueForRegName ra, sp, gp, tp, t0, t1, t2, s0, s1, a0, a1, a2, a3, a4, a5
	forall clearValueForRegName \regs
	forall zeroIfIncluded ra, sp, gp, tp, t0, t1, t2, s0, s1, a0, a1, a2, a3, a4, a5
.endm

/**
 * Load a capability in PCC from the specified symbol into the specified
 * register.
 */
.macro LoadCapPCC register symbol
1:
	auipcc		\register, %cheriot_compartment_hi(\symbol)
	cincoffset x0, x0, x0 // Mandatory 4-byte nop
	clc			\register, %cheriot_compartment_lo_i(1b)(\register)
.endm

/**
 * Load a capability in CGP from the specified symbol into the specified
 * register.
 */
.macro LoadCapCGP register symbol
1:
	auicgp		\register, %cheriot_compartment_hi(\symbol)
	cincoffset x0, x0, x0 // Mandatory 4-byte nop
	clc			\register, %cheriot_compartment_lo_i(1b)(\register)
.endm

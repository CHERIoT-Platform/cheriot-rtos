// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <cheri.hh>
#include <stddef.h>
#include <stdint.h>

struct TrustedStackFrame
{
	/**
	 * Caller's stack pointer, at time of cross-compartment entry, pointing at
	 * switcher's register spills (.Lswitch_entry_first_spill and following).
	 *
	 * The address of this pointer is the (upper) limit of the stack capability
	 * given to the callee.
	 */
	void *csp;
	/**
	 * The callee's export table.  This is stored here so that we can find the
	 * compartment's error handler, if we need to invoke the error handler
	 * during this call.
	 */
	void *calleeExportTable;
	/**
	 * Value indicating the number of times that this compartment invocation
	 * has faulted.  This is incremented whenever we hit a fault in the
	 * compartment and then again once it returns.  This means that the low bit
	 * indicates whether we're currently processing a fault.  A double fault
	 * will forcibly unwind the stack.
	 */
	uint16_t errorHandlerCount;
};

/**
 * Each thread in the system has, and is identified by, its Trusted Stack.
 * These structures hold an activation frame (a TrustedStackFrame) for each
 * active cross-compartment call as well as a "spill" register context, used
 * mostly for preemption (but also as staging space when a thread is adopting a
 * new context as part of exception handlng).
 */
template<size_t NFrames>
struct TrustedStackGeneric
{
	void  *mepcc;
	void  *cra; // c1
	void  *csp; // c2
	void  *cgp; // c3
	void  *ctp; // c4
	void  *ct0; // c5
	void  *ct1; // c6
	void  *ct2; // c7
	void  *cs0; // c8
	void  *cs1; // c9
	void  *ca0; // c10
	void  *ca1; // c11
	void  *ca2; // c12
	void  *ca3; // c13
	void  *ca4; // c14
	void  *ca5; // c15
	void  *hazardPointers;
	size_t mstatus;
	size_t mcause;
#ifdef CONFIG_MSHWM
	uint32_t mshwm;
	uint32_t mshwmb;
#endif
	uint16_t frameoffset;
	/**
	 * The ID of the current thread.  Never modified during execution.
	 */
	uint16_t threadID;
	// Padding up to multiple of 16-bytes.
	uint8_t padding[
#ifdef CONFIG_MSHWM
	  12
#else
	  4
#endif
	];
	/**
	 * The trusted stack.  There is always one frame, describing the entry
	 * point.  If this is popped then we have run off the stack and the thread
	 * will exit.
	 */
	TrustedStackFrame frames[NFrames + 1];
};
using TrustedStack = TrustedStackGeneric<0>;

#include "trusted-stack-assembly.h"

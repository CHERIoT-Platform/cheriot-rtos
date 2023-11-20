// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <cheri.hh>
#include <stddef.h>
#include <stdint.h>

struct TrustedStackFrame
{
	/// caller's stack
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

template<size_t NFrames>
struct TrustedStackGeneric
{
	void  *mepcc;
	void  *c1;
	void  *csp;
	void  *cgp;
	void  *c4;
	void  *c5;
	void  *c6;
	void  *c7;
	void  *c8;
	void  *c9;
	void  *c10;
	void  *c11;
	void  *c12;
	void  *c13;
	void  *c14;
	void  *c15;
	void  *hazardPointers;
	size_t mstatus;
	size_t mcause;
#ifdef CONFIG_MSHWM
	uint32_t mshwm;
	uint32_t mshwmb;
#endif
	uint16_t frameoffset;
	/**
	 * Flag indicating whether this thread is in the process of a forced
	 * unwind.  If so, this is one, otherwise it is zero.
	 */
	uint8_t inForcedUnwind;
	// Padding up to multiple of 16-bytes.
	uint8_t padding[
#ifdef CONFIG_MSHWM
	  13
#else
	  5
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

static_assert(
  CheckSize<COMPARTMENT_STACK_PERMISSIONS,
            CHERI::PermissionSet{CHERI::Permission::Load,
                                 CHERI::Permission::Store,
                                 CHERI::Permission::LoadStoreCapability,
                                 CHERI::Permission::LoadMutable,
                                 CHERI::Permission::StoreLocal,
                                 CHERI::Permission::LoadGlobal}
              .as_raw()>::value);

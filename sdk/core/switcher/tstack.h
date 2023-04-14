// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <cheri.hh>
#include <stddef.h>
#include <stdint.h>

struct TrustedStackFrame
{
	/// caller's PCC of a compartment call
	void *pcc;
	/// caller's globals
	void *cgp;
	/// caller's stack
	void *csp;
	/**
	 * Caller's callee saved registers. It's convenient to save it in the
	 * trusted stack frame, but a generic way should save them on the caller's
	 * stack, especially when the ABI has a lot of callee saved registers.
	 */
	void *cs0;
	void *cs1;
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
	// padding to make up to multiple of 16 bytes
	uint16_t padding[7];
};

template<size_t NFrames>
struct TrustedStackGeneric
{
	void    *mepcc;
	void    *c1;
	void    *csp;
	void    *cgp;
	void    *c4;
	void    *c5;
	void    *c6;
	void    *c7;
	void    *c8;
	void    *c9;
	void    *c10;
	void    *c11;
	void    *c12;
	void    *c13;
	void    *c14;
	void    *c15;
	size_t   mstatus;
	size_t   mcause;
	uint16_t frameoffset;
	/**
	 * Flag indicating whether this thread is in the process of a forced
	 * unwind.  If so, this is one, otherwise it is zero.
	 */
	uint8_t inForcedUnwind;
	// Padding up to multiple of 16-bytes.
	uint8_t  pad0;
	uint16_t padding[2];
	/**
	 * The trusted stack.  There is always one frame, describing the entry
	 * point.  If this is popped then we have run off the stack and the thread
	 * will exit.
	 */
	TrustedStackFrame frames[NFrames + 1];
};
using TrustedStack = TrustedStackGeneric<0>;

#include "trusted-stack-assembly.h"

// Require the trusted stack to be a multiple of 16 bytes. We could get away
// without this except that the loader rounds the stack size up to a multiple of
// 16 bytes and it confuses the overflow check in the switcher if the allocated
// stack length does not match the expected length.
static_assert((sizeof(TrustedStack) % 16) == 0);
// We don't want the above to depend on the number of trusted stack frames
// allocated so we'd better make sure TrustedStackFrame is also a multiple of 16
// bytes.
static_assert((sizeof(TrustedStackFrame) % 16) == 0);

static_assert(
  CheckSize<COMPARTMENT_STACK_PERMISSIONS,
            CHERI::PermissionSet{CHERI::Permission::Load,
                                 CHERI::Permission::Store,
                                 CHERI::Permission::LoadStoreCapability,
                                 CHERI::Permission::LoadMutable,
                                 CHERI::Permission::StoreLocal,
                                 CHERI::Permission::LoadGlobal}
              .as_raw()>::value);
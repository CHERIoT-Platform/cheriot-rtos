Privileged Libraries in the CHERIoT RTOS
========================================

Privileged Libraries are a mechanism in CHERIoT that allow libraries to hold loader-provided sealing keys.
This enables unprivileged compartments to perform certain privileged operations without the need for a full compartment switch.
Avoiding the switcher and its associated overheads altogether is desirable from a performance perspective, but requires care that we do not leak secrets.

Mechanics of Privileged Libraries
---------------------------------

Just as CHERIoT libraries are compartments without a data segment, Privileged Libraries are privileged compartments without a data segment.
A privileged compartment is a compartment that may have up to three sealing keys placed before its import table, accessible via PC-relative addressing.
Privileged compartments are built with a different linker script that reserves space for the key slots, which the loader then populates at boot.
The Allocator, the Scheduler, and the Software Revoker are all examples of such privileged compartments.

Library calls in CHERIoT differ from compartment calls in that they do not need to go through the switcher.
Instead, the calling compartment has a sentry capability to the library in their import table which can be invoked directly.
Since libraries lack a data segment, they cannot maintain global state, and are semantically equivalent to simply having a copy of the library code within the calling compartment.

Despite this semantic equivalence, the abstraction affords us some protection.
Since sentries are sealed capabilities, they cannot be dereferenced or used for pointer arithmetic.
The caller's only handle into the library is the sentry, so there is no way to derive a pointer into the library's PCC region and access the sealing keys that reside there outside the context of a call.

There is one subtlety to this, however.
Privileged libraries are libraries, and the compiler provides no guarantee that all registers are cleared on return from a library call.
Ordinarily this is handled by the Switcher, but since privileged library calls bypass it entirely, secrets may be left visible to the caller in uncleared registers
Due to this risk, privileged libraries like the Token Library are small, written in carefully audited assembly, and must manually manage their register state to avoid such leaks.
Privileged library calls also run with interrupts disabled, preventing an interrupt handler from observing register contents mid-call.
The need for hand-written assembly to manage register state could in principle be addressed by a compiler that guarantees registers are cleared on return from a library call.

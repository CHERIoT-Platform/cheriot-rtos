Error Handling
==============

*Fault isolation* is one of the key guarantees for compartmentalisation.
It must be possible to prevent a fault in one compartment from propagating to the rest of the system.
We provide mechanisms for doing this built on top of the CHERI security guarantees.

The trusted stack
-----------------

In the C/C++ abstract machine, structured control flow is modeled with a stack.
Every function invocation pushes a frame onto this stack and every return pops a frame.
The contents of the stack is opaque from the perspective of the abstract machine and is a property of the implementation.

We provide a similar abstraction for cross-compartment calls.
Every cross-compartment call pushes a new frame onto a *trusted stack*, which allows the switcher to return to the caller even if the callee's state is completely corrupted.
The error handling mechanism is built on top of this.

Faults that can be handled
--------------------------

On RISC-V, we allow any of the following faults to be handled by user code:

 - Misaligned data or instruction addresses
 - Access faults (accessing parts of the physical address range that don't exist)
 - Illegal instructions
 - Breakpoints
 - CHERI faults

In particular, note that CHERI faults happen as soon as an attempt is made to violate *language-level* memory safety.
For example, an out-of-bounds or invalid-permission fault is triggered before any invalid memory access.
Similarly, use-after-free bugs will appear as attempts to use an untagged capability.
In contrast, a segmentation fault on an MMU- or MPU-based system will occur only when an overwrite (or read) spans a protection domain.
For example an out-of-bounds write may overwrite the entire heap on an MPU-based system before triggering a fault when it attempts to write past the end of the heap, whereas the CHERI fault will be delivered as soon as the write attempts to go beyond the end (or start) of the object (or structure field, if sub-object bounds are used).

This means that the program should be in a defined state, according to the C abstract machine, when a fault is detected.
An attacker (or a simple bug) may have caused data corruption, but this will not propagate outside of objects that are reachable for the compartment and will not cause integers to be interpreted as pointers.

Handling faults
---------------

Compartments opt into local error handling by implementing the following function:

```c
enum ErrorRecoveryBehaviour compartment_error_handler(struct ErrorState *frame,
                                                      size_t             mcause,
                                                      size_t             mtval);
```

Note that this must be declared `extern "C"` in C++.
The first argument is a pointer to the register file, spilled on the stack.
All registers are saved as-is, with the exception of the program-counter capability (PCC).
PCC is stored untagged, to prevent leaking secrets from shared libraries (shared libraries that contain secrets should ensure that they are held in registers only for instructions that cannot fault).
The remaining arguments contain the values copied from the corresponding privileged registers, which allows the handler to identify the cause of the exception.

The error handler may modify the spilled register file in place and then returns either `InstallContext` or `ForceUnwind`.
If it returns `InstallContext` then the switcher will re-derive a PCC from the compartment's PCC and the address in the spilled frame, install all other registers, and jump to this address.
If it return `ForceUnwind` then the switcher will return to the previous compartment.

If a compartment has an error handler installed and calls a compartment that unwinds (either as the result of a fault that is not handled, or as a result of returning `ForceUnwind` from an error handler) then its error handler will also be invoked.
This will provide the CHERI fault value for `mcause` (0x1c) and a zero value for `mtval`.
This can be used to set global variables, or alter the return location when a called function is encountered.

If a compartment does not install an error handler then it cannot detect forced unwind events.

On any forced unwind, the return registers will be set to -1 and 0, respectively.
This means that any function returning an integer or a capability will see an untagged value of -1 as the result in the caller.
It is a good idea to avoid returning -1 to indicate success.

Limitations
-----------

The error handler runs on the compartment's stack and so cannot run if there is insufficient stack space.
This means that it cannot handle out-of-stack conditions.

Security Considerations
-----------------------

Shared libraries can contain secrets in globals and avoid leaking them if they are careful not to leave copies of them on the stack or in registers.
In the presence of error handling, they must also be careful to avoid storing them in registers during any operation that may trap (in particular, loads and stores should check that they have the required permissions first).

Compartment that can run out of stack space will unwind unconditionally.
This means that any caller-controlled value that can influence stack depth in the callee can be used to force the callee to unwind without running any cleanup code.
Note that the callee can still clean this up on the next entry, as long as it is tracked in globals.

Compartments that do not install error handlers cannot differentiate between a cross-compartment call returning -1 and returning as the result of a forced unwind.
This is normally not a problem because few functions return -1 to indicate success.

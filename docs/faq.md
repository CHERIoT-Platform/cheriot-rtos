Frequently asked questions
==========================

A small collection of questions and answers.

## Why C++ and not Rust?

Three reasons.
First, very little of our core system can be written in safe Rust:

 - The switcher must be assembly code, it requires explicit control over the stack and register file.
 - The memory allocator is responsible for carving up memory into objects and managing lifetimes, which is intrinsically not a memory-safe (let alone type-safe) operation.

The scheduler is responsible for cross-thread communication and synchronisation and requires to have objects on multiple queues simultaneously, which requires `unsafe` in Rust.

Second, CHERI Clang is fairly battle-tested at this point, having successfully compiled over a hundred million lines of C/C++ code for Morello.
There are efforts to bring up a Rust compiler for CHERI and which are now at a stage where everything that we need works but that are not yet nearly as well tested.

Third, the majority of the IoT / embedded ecosystem is still C code.
It is generally difficult to persuade embedded developers to move away from C to *anything* and so we need very tight interoperability with C.
C++ makes this easier than Rust at the moment.

We aim to support running Rust code in compartments as soon as the Rust compiler is sufficiently mature.

## Why C++ and not C?

We make use of a lot of features of the C++ static type system for compile-time checks.
For example, we provide a `constexpr` class representing a set of permission on a capability, which allows us to get compile-time errors if we try to derive a capability from a root that doesn't have that permission.

C++ templates give us type-safe logging and, in combination with `constexpr` give us a flexible way of implementing assertions and invariants with rich error messages in debug configurations.

## I don't want to use your build system for a compartment, how do I use my own?

When compiling code for a compartment, you must pass the `-cheri-compartment={compartment name}` flag to the compiler.
Beyond that, you just need the flags to specify the target and ABI: `-target riscv32cheriot-unknown-cheriotrtos -mcpu=cheriot -mabi=cheriot -mxcheri-rvc -mrelax -fshort-wchar`.

When linking a compartment, you must use the [compartment linker script](../sdk/compartment.ldscript), which specifies how to lay out the code and data for a compartment.
The resulting compartment can then be linked into a firmware image.

## How big is the TCB?

The loader runs with the root capabilities.
Between them, the loader is able to read, write, or execute all of the memory and MMIO space and is able to seal capabilities with any object type.
The loader erases these powerful capabilities after it finishes running and does not call any untrusted code or operate on any data other than the firmware image.
The result of running the loader is deterministic and so it is possible to dump the memory contents after the loader in a system with debug interfaces has run and perform arbitrary verification.
On a system that could store tag bits in persistent storage (e.g. Flash), the equivalent of the loader would run at firmware image build time and so this would not be part of the TCB.

After the loader exits, nothing in the system is omnipotent.
The most privileged component is the switcher, which is responsible for transitions between threads and between compartments.
This is around 300 instructions.
It runs with the access-system-registers permission and so is able to access the register that contains the pointer to the current thread's trusted stack.
The switcher is still constrained by the capability rules and so compromising it does not automatically give complete control, but almost certainly provides sufficient primitives that an attacker who compromises the switcher can eventually gain full control.

The switcher invokes the scheduler by sealing the capability to the current register save area and trusted stack and passing it to the scheduler, running on a separate stack.
The scheduler is fully in the TCB for availability (it decides which thread to run next and can trivially break availability by choosing never to run that thread) but not for confidentiality or integrity.
The scheduler does not see the contents of the stack or registers of threads that are interrupted.
All other entries to the scheduler are via a normal cross-compartment call, which ensures that the scheduler sees only explicitly-passed arguments.

The allocator is responsible for providing a shared heap.
It is responsible for setting bounds on capabilities to heap allocations, for setting the revocation bits on freed memory, and for not reusing memory until after revocation.
This means that a bug in the allocator can violate spatial or temporal memory safety *for heap memory*.
Bugs in the allocator cannot violate memory safety for compartment globals or stack allocations.

The scheduler and memory allocator are referred to in the code as 'privileged' compartments for one reason.
They are each given capabilities with permit-seal and permit-unseal permissions on one of the (seven) hardware sealing types.
The allocator uses this to provide a sealing service, the scheduler uses it to avoid trusting the allocator for inter-thread communication.

Revocation is implemented as either a hardware state machine or a software service.
In both cases, it is able to clear the tag bits on any capability anywhere in a range of memory that is provided to it.
The software implementation's revocation looks like this (run with interrupts disabled):

```c++
for (int i = offset; i < end; i++)
{
	current[i] = current[i];
}
```

This is easy to audit and, as long as the hardware implements the correct read barrier check, will invalidate all dangling capabilities in the provided range.

## What is the difference between a compartment and a shared library?

Compartments contain mutable state, shared libraries do not.
Compartments are considered separate security contexts, shared libraries are treated as being part of the security context of the compartment that invokes them.

While shared libraries can contain read-only data (in regions bounded by the PCC), these cannot be exported by the libraries.
They have to be explicit functions accessing the read-only data.

In terms of implementation, this means that a shared library does not require a full compartment transition to enter.
Calls to shared-library functions are simply indirect function calls, the overhead is 2-3 instructions above a normal in-compartment call.
Calls to functions in another compartment require register zeroing and stack zeroing, which can cost around 400 cycles.
In exchange for this, compartments have mutable globals that are protected from callers.

## What security guarantees do I get from a compartment?

Nothing outside of the compartment can run any of the compartment's code except by calling one of the compartment's public entry points.
Nothing outside of the compartment can access any of the compartment's globals unless pointers to them are explicitly passed as arguments in a call to another compartment.

This means that anything (code or globals) in a compartment has confidentiality and integrity guarantees.

## What security guarantees do I get from cross-compartment calls?

Only values that are explicitly passed as arguments to the callee are visible to the callee.
Only values explicitly returned from the callee are propagated back to the callee.
Any capability (pointer) that lacks global permission that is passed as an argument may be used by the callee, but cannot be captured.

## What memory-safety guarantees do I get?

Inter-object bounds errors will deterministically trap.
Note that the bounds of an object may include padding (for example, a 12-byte object may reside in a 16-byte allocation) but the bounds are guaranteed not to overlap with any other allocation.
Bounds information is carried with pointers.

Pointers cannot be forged.
All pointers in the system must be derived from existing ones.
Some that sit below the C/C++ abstract machine, such as the global pointer and the stack pointer, are powerful.
Assembly code can write over any part of the stack (except the part used by the caller of a cross-compartment call) and any globals in the current compartment, as can any C/C++ code that reads these registers with inline assembly.
The threat model for memory safety assumes that a compartment is free to do whatever it wants with its own globals and its own portion of the stack, but must not violate memory safety for objects created in other compartments.

Use-after-free bugs will deterministically trap.
Specifically, loading a pointer to a deallocated object will result in an untagged value (note: this is not safe to test with interrupts enabled because an object can be freed during a context switch).
Any attempt to access memory with an invalid pointer will trap.

After the loader has run, there are no memory regions to which both writeable and executable capabilities exist.
This may be relaxed in the future to permit dynamic code generation on the device.

## How do you prevent someone from spoofing a compartment or thread IDs?

The system does not have a notion of a compartment or thread ID, it is a capability system, not an access control list (ACL) system.
This is a key part of the principle of intentionality.
It is not sufficient for an entity in the system to be permitted to do something, that entity must actively choose to exercise that right.
This avoids common categories of confused deputy attacks.

Consider a system with four entities:

 - C1 and C2 are consumers of some service.
 - S is a provider of that service.
 - P is a privileged entity that performs some task.

C1 and C2 both call into S to perform some action.
As part of that action, S may call into P to perform the privileged action.
In an ACL-based system, a call to P from S will succeed simply because S is authorised to perform the privileged action.
If C1 is supposed to be allowed to ask S to call P, but C2 is not, then this is difficult to express because P cannot tell the difference between S-on-behalf-of-C1 and S-on-behalf-of-C2.

In a capability system, the call to S will take an optional capability authorising the holder to perform some action with the resource that P protects.
S will then pass that to P and P will perform or not perform the action based on a capability check.

At the lowest level, CHERI systems (including CHERIoT) perform this kind of check for every access to memory.
It is not sufficient that I have access to two objects, I must choose to access each object with a valid pointer *to that object*.
If I do some pointer arithmetic on a pointer to one object and happen to arrive at an address for another object, I may not use the resulting pointer access that other object *even though I also hold a valid pointer to the second object*.

CHERIoT builds on top of the hardware-enforced notion of capabilities with the token APIs in the allocator that allow arbitrary software-defined capabilities.
These can be used to represent any action that requires authorisation, for example the right to establish a TCP connection to a particular address.

Removing the global permission makes it possible to delegate these rights for the duration of a call so, in the above example, C1 can pass a capability to S, which S can pass to P, but which S cannot store anywhere other than its stack.
This ensure that if C1 and C2 both call S from different threads there is no way for S to accidentally invoke P with the wrong set of rights.

This approach also respects the principle of least privilege: S acting on behalf of C1 holds only the rights necessary to perform its action on behalf of C1, not the set of rights that it would need to hold on behalf of any caller.

## What C++ features can I use?

We provide a fairly rich freestanding C++ environment, without exceptions or RTTI.
Thread-safe `static` initialisers are supported in firmware images that use the `cxxrt` library.

Global constructors are not run automatically (there is no global initialisation phase) but a compartment can call `GlobalConstructors::run()` to run them.
This function is safe to call multiple times, it will run the constructors only once.

Compile-time features such as `type_traits` are all expected to work.
We provide an increasing subset of the optional components of the C++ standard library, for example `std::vector`.

Dynamic memory allocation is fully supported.


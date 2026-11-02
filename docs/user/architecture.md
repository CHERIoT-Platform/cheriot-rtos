CHERIoT RTOS architecture
=========================

The CHERIoT platform is designed around the principles of least privilege and intentionality.
As such, the structure is somewhat different from a conventional OS or RTOS, though has some things in common with a microkernel system.

Terminology
-----------

This document uses several terms that have concrete meanings in the context of a CHERIoT system:

 - A *CHERI capability* (often, for brevity, referred to simply as a *capability*) is an unforgeable value that authorises the hardware to perform some operation (typically on memory) when presented as an operand to an instruction.
 - A *compartment* is an isolated software component that contains some code and (optionally) some globals.
 - A *thread* is a schedulable entity that owns a register state and a stack, which can execute code.
 - A *firmware image* is a complete memory image of all of the code and data for the initial state of the system.
 - A *trusted stack* is a stack managed by the system that contains activation records for cross-compartment calls, which is protected from compartments.

Key abstractions
----------------

The CHERIoT platform provides two key abstractions: threads and compartments.

Compartments define spatial ownership.
A compartment defines a set of code, a set of globals, a set of exported entry points, a set of imported entry points  from other compartments, and a set of imported memory-mapped I/O (MMIO) regions.
A compartment is treated as a single protection domain.
While a compartment is running, it is assumed to be able to execute any of its code, modify any of its globals, access any of the MMIO regions that it has access to, or transfer execution to any of the compartments whose entry points it has imported.

Threads define temporal ownership.
A thread defines a register state (either in the register file while running, or in a register save area when preempted), a stack, and a trusted stack.
Threads are always executing in one compartment and that compartment has access to a subset of the stack
The trusted stack maintains a record of the compartments that a thread has suspended by performing a cross-compartment call and which will be resumed when those calls return.
Each cross-compartment call shrinks the stack so that all state created by existing compartments is inaccessible to the new callers.

The core components
-------------------

There are four core components in the system.

 - The *loader* runs when a firmware image is loaded into memory and sets up the initial capabilities.
 - The *switcher* is responsible for all cross-compartment and cross-thread transitions.
 - The *scheduler* is responsible for deciding which thread to execute when a thread yields or is interrupted and manages inter-thread communication mechanisms.
 - The *allocator* is responsible for managing a shared heap.

### The loader

The loader starts executing with the three root capabilities that grant execute and write access to all of memory and sealing access to the full sealing type space.
It does not execute on any untrusted data and erases itself after it finishes.
In a system with tag bits in EEPROM memory, the loader would not be necessary and the firmware image would contain something equivalent to the result of running the loader on the current firmware image.
Our system is intended to support loading firmware images from conventional flash memory or from a host processor and so does not assume tags.

### The switcher

The switcher is the most privileged component that runs with access to untrusted data.
It has two main entry points, one that is triggered on interrupt delivery and one that is invoked explicitly on cross-compartment calls.
The majority of the switcher runs with interrupts disabled, to simplify auditing.

The switcher runs with the access-system-registers permission.
It uses this permission to store a capability to the current thread's trusted stack and register-save area in the `mtdc` capability special register (CSR).

On interrupt, the switcher is responsible for saving the register state and invoking the scheduler with a sealed capability to the register save area and trusted stack.
On cross-compartment call, the switcher is responsible for unsealing the target, shrinking the stack to include only the unused parts, clearing the stack, setting up the new entry on the trusted stack.
On return, it performs these operations in reverse.

For some interrupt causes (synchronous exceptions), these two responsibilities become intertwined.
These faults can be handled by the compartment and so the switcher's interrupt entry point must examine the trusted stack, invoke a compartment, and potentially unwind the trusted stack in the same way that a normal compartment unwind occurs.

### The scheduler

The scheduler performs a lot of the tasks that a kernel would in a conventional system.
Unlike a conventional kernel, it is not more privileged than the threads that it schedules.
The scheduler must provide two entry points.
The first is called after the loader exits and runs on the thread that will become the idle thread.
This is provided with an array of thread metadata, including sealed capabilities to each thread's trusted stack and the threads' priorities.
The second entry point is invoked by the switcher and always runs on a separate stack.

The scheduler is just a compartment.
As such, it can export other entry points.
The switcher provides a number of communication primitives:

 - Message queues, which allow fixed-sized messages to be sent from one thread to another, with blocking and non-blocking sends and receives.
 - Event channels, which allow up to 24 different events to be waited per channel, with each wait blocking until either (at the caller's choice) all or some of the bits are set.
 - Futexes, where a caller can do an atomic compare-and-sleep or a wake on a 32-bit integer in memory.

Each of these involves a cross-compartment call to the scheduler.
The scheduler can then inspect and modify the current thread's state, including putting it on a sleep queue and marking it as waiting for a specific event, and then yielding.
Yielding is implemented with the `mcall` instruction, which enters the switcher and then invokes the scheduler's interrupt entry point, which may then schedule a new thread to run.

Most of the scheduler's entry points run with interrupts disabled.

### The allocator

The memory allocator is currently based on dlmalloc and is a simple range-based allocator.
The hardware provides a load barrier that, on loads of capabilities to the memory region that can be used as a heap, checks the revocation bit corresponding to the base of the capability and clears the tag if it is set.

On allocation, the allocator is responsible for finding some memory that is not currently in use and creating a capability that is bounded to that space.
On deallocation, the allocator is responsible for painting the revocation bits corresponding to an entire allocation, so that any attempt to load capabilities derived from the pointer returned from the allocation entry point will give an untagged value.
In between deallocation and reallocation, the allocator is responsible for ensuring that a complete revocation pass has scanned all mutable memory and deleted all pointers to the memory range.

In addition to memory allocation, the memory allocator also exposes a sealing API.
The CHERI hardware sealing mechanism allows creating sealed (opaque) capabilities that cannot be modified or used.
These sealed capabilities can be transformed back into the original with a `cunseal` instruction, with the correct authorising capability.
On 64-bit CHERI systems, there is typically at least an 18-bit field for the sealing type and so there is plenty of space for different uses.
On the CHERIoT platform, the type field is 3 bits and a system could easily have more than 7 different compartments wishing to use sealing.

The allocator virtualises the sealing abstraction.
Callers can request a new capability that can be used only with the allocator's sealing APIs (not with the `cseal` / `cunseal` instructions) for sealing and unsealing.
This can then be used to allocate memory via an API that returns both a sealed and an unsealed capability to the same object.
The bounds of the unsealed capability are slightly larger and include a header.
The allocator also provides an unseal API that allows a caller to request the unsealed capability if they present the unseal capability that matches the value in the header.

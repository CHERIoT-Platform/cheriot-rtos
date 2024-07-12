Memory Safety Examples
======================

This directory contains a set of examples to demonstrate how memory safety bug-classes are deterministically mitigated by the architecture.
This test is split into an [inner compartment](memory_safety_inner.c) that contains the examples and an [outer compartment](memory_safety_runner.c) that invokes it.
Each bug class faults in the inner compartment and unwinds into the outer one.
The bug-classes we demonstrate are covered by these three groups:

## Spatial safety

By "spatial safety" we mean that every memory allocation has a well-defined size/bounds, and it's incorrect to access memory outside of these bounds. 
Every C/C++ pointer in our system is a bounded, unforgeable CHERI capability.
Any dereference outside of an allocation's bounds, whether adjacent to the allocation or far outside, will trap. of an allocation's bounds, whether adjacent to the allocation or far outside, will trap.
This is enforced by the hardware and so the same guarantees apply to accesses from assembly code.
This property is enforced at the architectural level.
You can see examples for that in the cases for `StackLinearOverflow`, `HeapLinearOverflow`, and `HeapNonlinearOverflow`, [implemented](memory_safety_inner.cc:49) in the inner compartment.

## Temporal safety

By "temporal safety" we mean that it's incorrect to access a memory allocation outside of its lifetime.
Any C/C++ object has a lifetime defined by its *storage duration*.
For objects with automatic storage (informally, 'stack objects') this is the lexical scope.
Objects with static storage duration persist as long as the program runs.
Objects with dynamic storage duration (informally, 'heap objects') exist until they are explicitly freed.
The CHERIoT platform guarantees that temporal safety violations for objects with dynamic storage duration will trap.
It also guarantees that any attempt to capture an object with automatic storage passed from the caller in a cross-compartment call will trap.
The second property prevents cross-compartment temporal-safety violations for objects with automatic storage duration.
You can see an example for how UAF is deterministically mitigated by the load barrier in the case for `HeapUaf`, [implemented](memory_safety_inner.cc:91) in the inner compartment.

## Storing local pointers to globals

In our capability format we introduced special permission bits:
* `Global` (G), which is initially set on pointers to the heap and globals, and unset on the stack pointer.
* `StoreLocal`, which is only set on the stack pointer and means you're allowed to store pointers that don't have G set (as well as those that do).

There are no restrictions on where pointers to globals can be stored, but if a pointer to the stack is stored in the heap or a global then it is invalidated so that any attempt to use it will trap. 
The effect is that usable pointers to the stack can only be stored on the stack, and never in heap / globals.
It is also possible to clear G on global pointers, effectively stopping them from being captured during a cross-compartment call.
This adds a huge value to concurrent safety, because it means "local" memory (stack allocations, etc.) can't be used from memory accessible from different concurrent threads.
A great example for that is [implemented](memory_safety_inner.cc:113) in the case for `StoreStackPtrToGlobal`, implemented in the inner compartment.
Design of the allocator claims model
====================================

This document describes the design of the claim model that the allocator should provide and how it should be implemented.

The problem
-----------

Consider the scenario where compartment A passes a heap buffer to compartment B.
Another thread executing in compartment A may free the buffer in the middle of B's execution.
This will cause B to fault.
This is a concurrent operation and so sets the stage for classic TOCTOU problems.
In the core compartments, we mostly avoid this by running with interrupts disabled, but this is not desirable for the entire system.

To mitigate this, we need some mechanism that allows B to prevent the deallocation of an object.
This is further complicated by the fact that A and B may exist in a mutual distrust relationship and so B must not be permitted to consume A's memory allocation quota.

There are two variants of this problem, which may have different solutions.
In one case, the callee wishes to use an object for the duration of the call.
In another case, the callee wishes to use an object for a long period.
The main difference between these is the relative costs.
For objects held for the duration of a call, the cost of two calls into the allocator to hold and release the object may be prohibitive, whereas this is likely to be negligible for objects held for a long time.

Additional constraints
----------------------

In addition to solving the basic problem, we have a number of additional constraints on the design space:

 - A solution should be possible to apply at the boundaries when wrapping an existing component in a compartment.
 - Most objects will not use this mechanism (they will be reachable from a single compartment) and so there should be no overhead when not in use.
 - We are targeting resource-constrained systems and so the total overhead must be low.
 - We assume compartments may be malicious and so they should not be able to trick the allocator into creating a data structure that takes a very large amount of time to walk.

Possible approach 0: Explicit copies
------------------------------------

The first approach is simply to punt on the problem entirely.
When an OS kernel interacts with userspace memory, it calls explicit helpers to copy data to and from the kernel.
This is necessary because the userspace process may provide invalid pointers or update the memory map to invalidate pointers while the kernel runs.

In many situations, the same approach could work with CHERIoT.
We could provide a safe memcpy as a library function that runs with interrupts disabled, checks the source and destination pointers, and then performs the copy (up to some bounded size) and reports whether the copy succeeded.
This would be sufficient in a lot of cases but is not a very friendly programmer model.
In particular, it means that any compartment wrapping existing unmodified code would have to either (deep) copy any objects passed in or would have to make invasive changes to the wrapped code.

Note: This approach can be orthogonal to others and so we should implement it anyway.

Possible approach 1: Hazard pointers
------------------------------------

Hazard pointers provide inspiration for the first possible solution.
This approach involves having a per-compartment list of held objects that the allocator must consult before freeing an object.
The allocator would provide an API that allowed a compartment to register a table containing a (bounded) array of held objects and an allocation capability.
On deallocation, the allocator would need to traverse all such lists to determine whether an object is on any of them.
If the object is found on a list then the metadata should be updated to mark it as owned by the allocation capability associated with the list.

This is attractive for the fast-path case, because the compartment wishing to claim an object for the duration of a call needs only to insert a pointer to it into a pre-registered array.
It is unclear whether it is possible to use this interface securely.
In particular, a malicious compartment could allocate a large object and pass a pointer to a small sub-object to the callee.
The callee has no mechanism (other than calling the allocator) to know whether it holds a pointer to an entire object.

Possible approach 2: Explicit claims
------------------------------------

The approach in CheriOS is similar to reference counting.
Compartments may explicitly claim memory and objects may not be deallocated until all claims have been dropped.
Importantly, the CheriOS model handles resource constraints: when you claim an object, it counts towards your quota.

The down side of this model is that it requires tracking a lot of state.
Straight reference counting is not sufficient because references can be owned by mutually distrusting entities.
With a simple reference counting model, the original attack is still possible:

1. A allocates an object.
2. A passes a pointer to the object to B.
3. B increments the reference count.
4. A decrements the reference count twice from another thread.
5. B traps.

Each reference to the object is now state that must be tracked and, most likely, will require an allocation.
We currently have 14 bits spare in the allocation header (slightly more if we shrink the allocator ID, which does not actually require a full 16 bits, since a 16-bit allocator ID would require 2 MiB of RAM to hold all of the corresponding allocation capabilities, leaving no space for the heap) and so could store a heap-start-relative (shifted for alignment) pointer to the head of a linked list of claims.
In the common case, this field will be 0 (the start of the heap cannot be allocated) and so free can skip it.

The proposed solution involves constructing a linked list of structures holding an allocator ID, a saturating reference count, and a next pointer.
When an object is claimed, the allocator walks the list of claims to find a structure with a matching ID.
If one is found, it increments the reference count.
If not, then it allocates a new object (out of the caller's quota) to hold the claim and inserts it into the head of the list.
The size of the object is subtracted from the caller's quota.

When a claim is dropped (this does not require a new API, `heap_free` must do the same thing, which is convenient for destructors), the allocator has to handle two cases:

 - Is this an additional claim?
   If so then it will have a claim entry on the list.
 - Is this the original allocator?
   If so, then it will be the owner ID in the header.

These are not mutually exclusive.
The owner may claim its own object to avoid having to track whether it owns an object that is passed to it on a recursive call.

The second case is easy to handle, check the caller's allocator capability ID against the header as is done today.
Instead of automatically proceeding to deallocation if this succeeds, the allocator should free the object only if the claims list is also empty.
If the claims list is not empty, the owner is set to 0 (0 is an invalid value for an allocator ID).

To handle the first case, the allocator walks the claims list, decrementing the reference count when it finds the corresponding entry.
If the reference count saturates then the decrement is silently ignored.
If this reference count reaches zero, then the claim structure is deallocated (reclaiming both its usage and that of the object from the caller's quota).
If this is the last claim, then the object should be deallocated.

Note that this approach still has potential problems with sub objects.
We have a choice of whether to allow claims with capabilities that span less than the entire object.
If we do, then we still need to consume sufficient quota for the entire object, which may be surprising to the callee.
If we do not, then sub-object bounds (which are good for security) become less useful.

I propose that we implement an API with this signature:

```
size_t claim_object(struct SObjStruct *heapCapability, void *capability);
```

The return value of this is the number of bytes that were consumed from the quota.
If this is zero, the claim failed.
Any non-zero result indicates success but a caller can also check that the result is below some expected maximum.

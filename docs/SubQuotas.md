# Dynamically Allocated Sub Quotas

[Sub quotas](https://github.com/CHERIoT-Platform/cheriot-rtos/issues/140) provide a mechanism for delegating only a portion of a heap quota from one compartment to another compartment.
This is desirable, as currently, when a compartment delegates its quota, it does so in whole.
This leaves the compartment open to resource exhaustion attacks from the delegatee, or integrity violations via dangerous Allocator APIs like `heap_free_all`.
[Previously](https://github.com/CHERIoT-Platform/cheriot-rtos/pull/617), we implemented user-defined permissions on top of sealed capabilities to allow for the creation of less-privileged allocation capabilities, in an attempt to minimize the trust necessary to delegate, but this was not a complete solution.


For one, barring the usage of an allocation capability for specific Allocator APIs is overly restrictive: You, as a delegator, may want to allow the delegatee the ability to perform quick cleanup on this quota via `heap_free_all`, but don't want to have to trust that the delegatee will not free your allocations from under you.
There are also useful design patterns that sub quotas would enable, such as lending a portion of quota as collateral to a compartment that allocates objects for you.
The delegatee can use your quota to claim allocations made with its quota, ensuring you won't exhaust its resources without also exhausting your own.
You are also guaranteed the delegatee will not consume more of your resources than the portion explicitly shared.
Supporting such a relationship of mutual distrust is at the core of the CHERIoT compartment model.


Having the ability to dynamically allocate sub quotas solves these issues.
The pattern becomes:


```cpp
DECLARE_AND_DEFINE_ALLOCATOR_CAPABILITY(parentQuota, 1024 * 1024);
#define PARENT_QUOTA STATIC_SEALED_VALUE(parentQuota)

// ...

AllocatorCapability subQuota = split_quota(PARENT_QUOTA, 1024);

CHERI::Capability sealedSocket = network_socket_create_and_bind(
    timeout, subQuota, isIPv6, ConnectionTypeTCP);

fold_quota(PARENT_QUOTA, subQuota); // allocation now owned by PARENT_QUOTA

// ...

network_socket_close(timeout, subQuota, sealedSocket);
```

The exact semantics are discussed later in the document, but the main contribution is in allowing the programmatic creation of minimally sized quotas that can be passed into quota-requiring APIs to isolate the callers main quota from a potentially compromised compartment.
They establish an upper bound on the allocation done by the delegatee compartment, and prevent malicious usage of `heap_free_all` from compromising the rest of the delegator's allocations while still allowing its use within the delegatee, better respecting both the principle of intentionality and of least privilege.


## Design

At present, statically allocated quotas are assigned IDs lazily via a monotonically increasing counter, with a maximum of 2^13 = 8192 unique IDs.
There is no provisioning for ID reuse.
To that end, I propose running an allocator for quota IDs in the existing free space of `AllocatorCapabilityState` structs by threading them onto a sorted linked list.


To do so, each quota will additionally maintain:
- A doubly-linked list structure (prev/next pointers)
- Initially assigned quota (provides a fast-path for destruction)       
- (Optionally) A synchronization primitive to allow for protecting sub quotas during operations that drop the allocator lock.


The list is maintained in sorted order by identifier.
We use a next-fit allocation policy with a roaming `head` pointer to reduce clustering of IDs.


Since the AllocatorCapabilityState is 24 bytes in size, and only 48 bits are currently used, we have 16 bits to store the initial quota value and 16 bytes to describe our storage datastructure.


Using 16-bit compressed pointers for the linked list structure and a `futex` word for synchronization, we retain 8 bytes for future use while we incur no additional storage overhead to provide ID reuse.


### Handling Static Quotas

We know at build time exactly how many static quotas will ever exist.
We reserve the first n IDs for these, allowing them to still be lazily assigned as the quotas are used.
We store this counter in a static variable, as is done currently.


We do not thread static quotas onto the linked list, since the allocator is expected not to retain references to them after usage with its APIs.
Indeed, the allocator may only ever be passed a no-capture handle, and in such a case be fundamentally unable to track these quotas.


### Creation of a sub quota

Sub quotas are created via a call to `split_quota(parent, size)`, which splits off the requested size into a new child sub quota. This operation will fail if the required quota is not available, which includes the size of the sub quota object, as it will be allocated using the parents quota.
It has no effect on existing allocations on the parent quota.


Creation requires taking a new ID and updating the list of extant sub quotas.
We find the interval of free IDs between `head` and `head->next`.
If there is a free ID in the interval, we assign to our new quota the midpoint and set our new `head` to `head->next->next`.
If are no free IDs in the interval, we advance head and try again.


As long as the total number of active sub quotas stays low, allocation is a fast operation and should require chasing very few pointers.


### Destruction of a sub quota

Because a sealed allocation can only be freed by the compartment that holds the sealing key, and a delegated sub quota may be used to make sealed allocations, we cannot support a notion of destruction for sub quotas that forcibly frees all associated allocations.


Rather 'destroying' a sub quota, which would imply a freeing of then-orphaned allocations, we provide a `fold_quota(parent, child)` API.
This operation hands ownership over all associated allocations from the child to the parent quota.
The parent-child relationship can be verified by checking the ID of the heap chunk the child resides in.
To transfer ownership, the allocator must walk the heap and modify the owner ID of each associated chunk.


The parent assumes responsibility for freeing the allocations as it pleases.
Any API that takes a delegated allocator capability and uses it for sealed allocations is already expected to expose an API to free said allocations.
At which point delegator may pass a restricted copy of its parent capability that may only be used to free said allocations.


Claims made on objects allocated by the child sub-quota will be unaffected by the fold, but claims made by the child sub-quota will be transferred as other heap allocations are.
In the case that the object is already claimed by the parent, this operation will increase the refcount on that claim.
In the case that the object is owned by the parent and claimed by the child, or vice versa, this operation will act as a `heap_quota_claim` on an owned capability does--at present, converting the ownership to a claim (resulting in two claims on the object, and no owner).
While the parent can no longer directly free, the allocation is simply freed once the number of associated claims hits zero.


Sub-quotas that are themselves children of the child sub-quota will be similarly transferred to the parent quota, and can be treated as any other heap allocation.


This operation roughly inverts the process of creation in terms of its effect on the linked list.
We remove our node from the list and stitch the list back together.


## Concurrency

### Simplified

Allocator APIs may drop the lock midway through an operation, as in a call to allocate with a full heap.
In light of this, the simplest case for handling concurrency for sub quota objects is to insert a validity check for allocator capabilities any time the lock is reaquired, at which point the operation should fail if the quota is no longer valid.
This is likely sufficient for an initial implementation.
Splitting quota will require holding the allocator lock.


### Further Work

This design can be improved.
Since a fold operation requires a full heap scan, it is desireable to not block in-flight allocator operations behind it.
To support this behaviour, we need a way of preventing destruction of the sub quota object without the allocator lock held.


During a fold, multiple threads can have the quota in a live-state.
We need to guarantee that all in progress quota operations finish (we'd like them to complete) before the fold can continue.
We include a futex word in the quota objects to handle the sleep/wake mechanism.


Inside the futex word, we track a refcount and a flag.
Each quota operation, such as a call to `heap_allocate`, increments this refcount while in-flight, and decrements only when it finishes.
The fold operation sets the flag, which causes all subsequent heap API calls with this quota to fail.
It then waits on the futex word, and is woken when the refcount hits zero.


In this case, `fold_quota` proceeds as follows:
```
1. CAS set closing flag.
2. If CAS fails, abort. Other fold proceeding.
3. while true:
   3.1 wait on futex.
   3.2 if refcount == 0 and wait != EAGAIN then break.
4. acquire allocator lock.
5. proceed with fold.
```


This requires modification to allocator APIs that may drop the lock during their execution, such as allocation.
Note that since static quotas cannot be destroyed, we can ignore the futex in their case, using some sentinel value to signal statically allocated.


```
1. if dynamic:
    1.1 if closing flag set, abort.
    1.2 attempt to CAS incremented refcount.
    1.3 if CAS fails, loop. else we now hold refcount.
2. acquire allocator lock.
3. attempt allocation according to current `heap_allocate` semantics.
4. release the allocator lock.
5. if refcount == 0 and closing flag set and dynamic:
   5.1 decrement refcount.
   5.2 wake the folder.
```


### Further Further Work

As with `heap_free_all`, we'd like to be able to drop the allocator lock while executing a fold.
Refcounting handles the quota object side of this story, but we still run into the issue of dropping the lock, yielding, and waking up with an invalid position, since the heap structure may have been mutated.
To mitigate this issue, we require an extra bit in the header to mark a given chunk as an anchor for the heap walk.
Before dropping the lock during the walk, we set the `anchor` bit, which signals that, should the chunk be 'freed', it shouldn't be marked revoked.
Then, when our walk wakes back up, we handle the actual freeing of said chunk, and continue.


To avoid starvation in a scenario where a low-priority thread starts a heap walk while another frees a very large object, we can, in the free path, split off a minimally sized chunk of the large object to serve as the anchor.
We must also not allow a chunk to serve as the anchor for more than one heap walk at a time, so that we can always free once we resume our walk.

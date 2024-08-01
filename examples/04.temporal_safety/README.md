Temporal safety example
=======================

This example shows an number of use-after-free cases.

The first case simply allocates an object and frees it, printing the value of the pointer both before and after deallocation.

The output from this should be something roughly like this:

```
Allocating compartment: -- Simple Case --
Allocating compartment: Allocated: 0x80004ce0 (v:1 0x80004ce0-0x80004d0a l:0x2a o:0x0 p: G RWcgm- -- ---)
Allocating compartment: Use after free: 0x80004ce0 (v:0 0x80004ce0-0x80004d0a l:0x2a o:0x0 p: G RWcgm- -- ---)
```

The debug facilities pretty print CHERI capabilities, so lets look at these two in detail.
The first value is the address, which is the same in both.
Everything else, shown in brackets, is the *capability metadata*.

The only field that changes after the call to `free` is the *tag* or *valid* bit.
This is the bit that indicates whether this is a valid capability.
When this bit is cleared, the CPU will not permit this pointer to be used to authorise any operations.
Its fields can still be read, which is why the other lines are the same.

The next field shows the range that this capability authorises access to.
We haven't done any pointer arithmetic on this and so the base and the address are the same.
The length field, prefixed with `l:`, is simply the difference between the top and the bottom.

The object type field (`o:`) is part of the sealing mechanism.
If this is zero, then the capability can used for whatever operations its range and permissions authorise.
If it is non-zero then it can be used only for unsealing operations, with a suitable authorising capability.

The permissions field (`p:`) lists the permissions that this capability has.
These are:

 - `G`: this is a global capability, it may be stored anywhere.
 - `R`: load permission, you may use this to read from memory.
 - `W`: store permission, you may use this to write to memory.
 - `c`: capability access, load and store permissions on this authorise loading and storing capabilities in addition to data.
 - `g`: load global, capabilities loaded through this do not lose their global permission on load.
 - `m`: load mutable, capabilities loaded through this do not lose their store permission on load.

This is the expected set for heap memory.
Any of these can be cleared from a copy of the pointer before passing it to other code if you wish to restrict what can be done to an object with that pointer.
For example, if you remove `W` and `M` permissions from a pointer that you pass as a parameter then you have a guarantee that nothing reachable from the pointer will be mutated.
Similarly, if you remove `G` and `L` then you have the guarantee that nothing reachable from the pointer will be captured.
If you remove `G` but not `L` then you have the weaker guarantee that the pointer that you passed will not be captured but pointers reachable from it might be.

The next three use cases show the handling of sub object, a capability that references a sub-range of the allocation.

In the first of these the sub object is passed to free(), but this has no impact until the original allocation is also freed (i.e an allocation does not become fragmented if you try to free the middle of it).

In the second use case a claim is made on the sub object, which counts against the quota but now means that the sub object remains valid even if the enclosing allocation is freed.
Both the sub object and the enclosing allocation become invalid when the claim is releases. 

The third use case shows the difference between a claim and a fast claim.
Claims are persistent, count against the compartment's quota, and last until they are explicitly released, but they are expensive as they require a cross compartment call to the allocator.
Fast claims are ephemeral and belong to the thread rather than the compartment.
Each thread may only hold one fast claim (on up to two objects).
They do not count against a quota, but they only last until the thread makes a cross compartment call or another fast claim.
In the example the claim on the sub object is made with a fast claim, but when the enclosing object is now freed the fast claim is dropped (as free() is a cross compartment call) so both the enclosing and sub objects become invalid.
_This is a poor use of a fast claim used to illustrate the behaviour; The normal use case is to establish a claim early in the entry to a compartment to prevent an object becoming invalid while the compartment processes it, which may include making a persistent claim._    

The final use case shows how an object initially allocated in one compartment be claimed by (add counted against the quota) of a second compartment.
This allow, for example, a zero copy data buffer pattern.
Even when the allocation is freed by (and removed from the quota of) the original compartment it remain valid because it is now claimed by the second compartment.
Note that the quota is reduced by more when making a claim than an allocation because a small amount of additional heap is required for the claim headers.   


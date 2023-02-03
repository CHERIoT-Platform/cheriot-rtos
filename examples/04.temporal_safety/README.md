Temporal safety example
=======================

This example shows a trivial use-after-free bug.
The code simply allocates an object and frees it, printing the value of the pointer both before and after deallocation.

The output from this should be something roughly like this:

```
Allocating compartment: Allocated: 0x80004ce0 (v:1 0x80004ce0-0x80004d0a l:0x2a o:0x0 p: G RWcgm- -- ---)
Allocating compartment: Use after free: 0x80004ce0 (v:0 0x80004ce0-0x80004d0a l:0x2a o:0x0 p: G RWcgm- -- ---)
```

The debug facilities pretty print CHERI capabilities, so lets look at these two in detail.
The first value is the address, which is the same in both.
Everything else, shown in brackets, is the *capability metadata*.

The only field that changes after three call to `free` is the *tag* or *valid* bit.
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

Sealing opaque types
====================

It is very common in compartmentalised software to want to hand out a handle to some object to callers and later get it back with the guarantee that it has not been tampered with.
CHERI provides a sealing mechanism for this kind of use case.
A CHERI capability (pointer) can be sealed with another capability that represents a type.
The resulting pointer can be passed around as normal but any attempt to dereference it (or even perform arithmetic on it) will fail.
The holder of the capability with the permit-unseal capability and an address matching the sealed capability's type can unseal it and get back the original capability.
The unseal operation returns null if the sealed capability is not a valid sealed capability of the correct type.

The CHERIoT capability encoding does not leave much space for sealing types (3 bits) and so the RTOS provides an abstraction layer for this as part of the allocator APIs.
This example demonstrates using this with the most simple service: one that stores unforgeable immutable integers in opaque types.
The caller of this service can create a new identifier object containing an integer and get back an opaque pointer that they cannot access directly.
They can then call another function to get the value.

Note that this example is using C++ thread-safe static initialisers to lazily allocate the sealing key.
The build system adds the C++ runtime (the line that includes the cxxrt directory) to make these work.

When you run this example, you should see an output line that looks like this:

```
Caller compartment: Allocated identifier to hold the value 42: 0x800058e0 (v:1 0x800058e0-0x800058ec l:0xc o:0xb p: G RWcgm- -- ---)
```

Note the `o:0xb` in this output.
This field indicates the sealing type of the pointer and a non-zero value indicates that it is sealed.
The hardware prevents the caller from tampering with this pointer or its pointee.
You will also see an output line something like this:

```
Identifier service: Allocated identifier, sealed capability: 0x800058e0 (v:1 0x800058e0-0x800058ec l:0xc o:0xb p: G RWcgm- -- ---)
unsealed capability: 0x800058e8 (v:1 0x800058e8-0x800058ec l:0x4 o:0x0 p: G RWcgm- -- ---)
```

Note that the address and bounds of the sealed and unsealed capabilities are not the same.
This is because the allocator adds a header to the sealed capability to ensure that allocations created with one key can be unsealed only with that key.
The last line of output from the compartment shows that these sealed pointers are subject to the same temporal safety guarantees as other pointers: when you free one, it becomes impossible to load a valid pointer to the same sealed object.

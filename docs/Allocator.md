The CHERIoT RTOS memory allocator
=================================

The CHERIoT platform was designed to support a (safe) shared heap.
This means that embedded systems that contain multiple mutually distrusting components do not need to pre-reserve memory and so the total SRAM requirement is the peak of all concurrently executing components, not the peak of all components.
This can be a significant cost reduction for systems that have relatively high memory requirements for different phases of computation, for example doing a post-quantum key exchange at boot followed by running a memory-intensive loop after initialisation.

This document describes the memory model.

Allocator capabilities
----------------------

Allocating memory requires a capability that authorises memory allocation.
These are created by the `DECLARE_AND_DEFINE_DEFINE_ALLOCATOR_CAPABILITY` macros, which takes two arguments.
The first is the name of the capability, the second is the amount of memory that this capability authorises the holder to allocate.
This capability may then be accessed with the `STATIC_SEALED_VALUE`, which takes the name as the argument.
If you wish to refer to the same capability from multiple C compilation units, you can use the separate `DECLARE_` and `DEFINE_` versions of this combined macro.
See [the documentation on software-defined capabilities](SoftwareCapabilities.md) for more information.


Compartments may hold more than one allocation capability.
The design embodies the principle of intentionality: you must explicitly specify the quota against which an allocation counts when performing that allocation.

When inspecting the linker audit report for a firmware image, you will see an entry like this for each allocator capability:

```json
        {
          "contents": "00001000 00000000 00000000 00000000 00000000 00000000",
          "kind": "SealedObject",
          "sealing_type": {
            "compartment": "alloc",
            "key": "MallocKey",
            "provided_by": "build/cheriot/cheriot/release/cherimcu.allocator.compartment",
            "symbol": "__export.sealing_type.alloc.MallocKey"
          }
        },
```

The `contents` is a hex encoding of the contents of the allocator.
The first word is the size, so 0x00001000 here indicates that this capability authorises 4096 bytes of allocation.
The remaining space is reserved for use by the allocator (the object must be 6 words long).
The sealing type describes the kind of sealed capability that this is, in particular it is a type exposed by the `alloc` compartment as `MallocKey`.


Core APIs
---------

The allocator APIs all begin `heap_`.
The `heap_allocate` and `heap_allocate_array` functions allocate memory (the latter is safe in the presence of arithmetic overflow).
All memory allocated by these functions is guaranteed to be zeroed.
These functions will fail if the allocator capability does not have sufficient remaining quota to handle the allocation.
All allocations have an eight-byte header and this counts towards the quota, so the total quota required is the sum of the size of all objects plus eight times the number of live objects.

The amount of quota remaining in a allocator capability can be queried with `heap_quota_remaining`.

The `heap_free` function deallocates memory.
This must be called with the same allocator capability that allocated the memory (you may not free memory unless authorised to do so).
This function is also used to remove claims (see below).

Claims
------

In some situations it is important for a compartment to be able to guarantee that another compartment cannot deallocate memory that was delegated to it.
This is done with the `heap_claim` function, which adds a claim on the memory.
This prevents the object from being deallocated until the claim is dropped.
This requires an allocator capability because it can prevent an object from being deallocated and so can increase peak memory consumption in a system.
This function returns the size of object that has been claimed (or zero on failure) because the object can be larger than the bounds of the capability but there is no way to claim part of an object and allow the remainder to be freed.

Claims are dropped with `heap_free`, which allows cleanup code to relinquish ownership without knowing whether an object was allocated locally or claimed.
In particular, it is safe to claim an object that you originally allocated, as long as you free it the correct number of times.

Standard APIs
-------------

The C `malloc` and `free` and C++ `new` and `delete` functions are implemented as inline wrappers around the core functions.
These use the default malloc capability for the compartment, with a quota defined by the `MALLOC_QUOTA` macro.
This currently defaults to 4096 bytes.

These APIs are provided for compatibility.
They are not ideal in embedded systems or with mutual distrust because they do not take explicit allocator capabilities and because they do not provide timeouts (and so can block indefinitely).

We do not provide an implementation of `realloc` because it is dangerous in a single-provenance model.
Realloc may not do in-place size reduction usefully because there may be dangling capabilities that have wider bounds.
Doing length extension in place would cause problems with existing pointers being able to access only a subset of the object.
The only safe way of implementing realloc is as an allocate, copy, deallocate sequence and users are free to provide their own implementation that does this.

Restricting allocation for a compartment
----------------------------------------

Compartments can opt out of C malloc by defining `CHERIOT_NO_AMBIENT_MALLOC`.
This will prevent the `malloc` family of functions being exposed in the compartment.
It will also prevent the compartment from having a default allocator capability.
This makes it easy to audit the property that some compartments should not allocate memory: their linker audit report will not contain a capability of the form described above.

The C++ `new` / `delete` functions wrap `malloc` and friends.
These can be hidden by defining the `CHERIOT_NO_NEW_DELETE` macro.


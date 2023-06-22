Static sealed objects in CHERIoT RTOS
=====================================

CHERIoT is a capability system.
This extends from the hardware (all memory accesses require an authorising capability in a register, provided as an operand to an instruction) up through the software abstractions in the RTOS.
In a capability system, any privileged operation requires an explicit capability to authorise it.

Privilege is a somewhat fluid notion in a system with fine-grained mutual distrust but, in general, any operation that affects state beyond the current compartment is considered privileged.
This includes actions such as:

 - Acknowledging interrupts
 - Allocating heap memory
 - Establishing network connections

Each of these should require that the caller present a capability that authorises the callee to perform the action on behalf of the caller.

Delegation is an important part of a capability system.
A capability may be passed from compartment A to compartment B, which can then use it to ask C to perform some privileged operation.
The identity of the immediate caller does not matter.

CHERI capabilities to software-defined capabilities
---------------------------------------------------

CHERI provides a hardware mechanism for building software-defined capabilities: sealing.
The sealing mechanism allows a pointer (a CHERI capability) to be made immutable and unusable until it is unsealed using an authorising capability.
The CHERIoT ISA has a limited set of space for sealing types and so these are virtualised with the allocator's [token](../sdk/include/token.h) APIs.
These APIs combine allocation and sealing, returning both sealed and unsealed capabilities to an object.
The unsealed capability can be used directly, the sealed capability can be passed to other code and unsealed only by calling the allocator's `token_unseal` function with the capability used to allocate the object.

A compartment can use this to generate software-defined capabilities that represent dynamic resources.
For example, a network stack can use it to allocate the state associated with a connection.
The scheduler uses the same mechanism for providing capabilities for cross-thread communication so that, for example, only a holder of the relevant capability can send or receive messages in a message queue.

Static software-defined capabilities
------------------------------------

In addition to the dynamic software-defined capabilities, it is often useful to provision a set of capabilities to a compartment at build time.
The simplest case for this is (allocator capabilities)[Allocator.md], which authorise allocation and so are necessary to be able to create any dynamic software-defined capabilities.
The scheduler also uses this mechanism for capabilities that allow interaction with [interrupts](Interrupts.md).

Objects created in this way are allocated outside of a compartment's global region but are accessed only via capabilities provided by the loader.
These capabilities use the allocator's sealing mechanism and so can be unsealed only by the owner of the relevant sealing capability.

For more detail on how to use the static sealing mechanism, see [`compartment-macros.h`](../sdk/include/compartment-macros.h).

### Exporting a sealing type

The `STATIC_SEALING_TYPE` macro defines a new sealing type that the loader can use to seal static objects.
This macro also evaluates to the capability that permits unsealing.

Static sealing capabilities are defined by both their name and the name of the compartment that exports them and so the name that you pick does not need to be unique (though, for documentation purposes, it should not be too generic).

### Creating a sealed value

Static sealed values are created with the `DECLARE_STATIC_SEALED_VALUE` and `DEFINE_STATIC_SEALED_VALUE` macros.
These construct an object with the specified type and contents.

**Note:** These objects may not contain pointers.

### Auditing

Static sealed objects are visible to the linker and set up by the loader.
As such, they are amenable to auditing.
Each static sealing type will have an entry in the exports section of its compartment of the following form:

```json
        { 
          "export_symbol": "__export.sealing_type.static_sealing_inner.SealingType",
          "exported": true,
          "kind": "SealingKey"
        },
```

This example is taken from the test suite.
It gives the unique symbol name for the sealing key (which includes the compartment name and the key name) and indicates that it is a sealing key.

A static sealing value that uses this sealing type will look something like this:

```json
        { 
          "contents": "2a000000",
          "kind": "SealedObject",
          "sealing_type": {
            "compartment": "static_sealing_inner",
            "key": "SealingType",
            "provided_by": "build/cheriot/cheriot/release/static_sealing_inner.compartment",
            "symbol": "__export.sealing_type.static_sealing_inner.SealingType"
          }
        },
```

The `sealing_type` key contains information required to cross reference this with the export and to sanity check that it really does come from the compartment that you expect to provide it.
The `kind` will be `SealedObject`.
The `contents` is a hex string of the object value.

This can be used for both coarse-grained auditing (which compartments have a capability of this kind?) and fine-grained auditing (what, precisely, does the capability held by compartment X authorise?).

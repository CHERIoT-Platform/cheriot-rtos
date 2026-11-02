Firmware initialisation design
==============================

Issue [275](https://github.com/CHERIoT-Platform/cheriot-rtos/issues/275) tracks the fact that we need some way of running intialisation code.
This document is a proposal for a concrete design.

In-scope use cases
------------------

The issue tracks several use cases.
Two complex use cases are out-of-scope for this proposal:

 - Collecting all sealed objects of some type and initialising them.
   This can be built on top of this and is *almost* orthogonal.
 - Registering callbacks.
   This would require initialisation to make cross-compartment calls, which brings in some significant complexity.

The most important ones are related to device initial configuration, running global constructors, and so on.

High-level design
-----------------

Like the `.init_array` model in ELF, we create a sorted-by-priority array of -'function pointers', which are actually cross-compartment callback pointers (sealed capabilities to export tables) points to each entry point.
After the C++ phase of the loader has run, the assembly stub will iterate over this list, invoking each on the stack that will eventually become the stack for the scheduler, with a one-deep initial trusted stack.

This reuses the existing switcher machinery for isolation and should be simple to implement.

The issue originally proposed that we would not need any kind of ordering because not being able to make cross-compartment calls meant that they would be independent.
Unfortunately, that is not the case because they *can* do device initialisation.
In particular:

 - A call setting up the UART for debug output would ideally be run before other things that might want to provide debug output.
 - Clock configuration to run the core in a high-speed mode would ideally be run before things that do anything CPU-intensive.

Declaring an init function
--------------------------

A function that is used as an init function should be marked as a CHERIoT callback.
This will place it in the export table, with hidden visibility.

Eventually, we should add an attribute like `__attribute__((constructor(priority)))`, but for now we can use a macro that uses some inline assembly, of the form:

```c
CHERIOT_INITIALISER(function, priority)
```

This will expand to something in a `.cheriot_initialisers.{priority}` section that is initialised in the same way that an import-table entry would be.

Loader structure
----------------

The loader will see an additional section to contain all of the initialiser callbacks.
It will initialise this as if it were another import table, with the constraint that it must contain *only* cross-compartment calls.
This will be a new section in the default linker script that is sorted using the `SORT_BY_INIT_PRIORITY` linker script directive.

After running the C++ part of the loader, the assembly part will loop over the array and invoke the switcher for each entry.
The assembly portion can construct the pointer to the array directly, because the C++ port modifies it in place.
The assembly portion must also receive the pointer to the switcher, via the `SchedulerEntryInfo` output argument.

Loader trusted stack changes
----------------------------

The loader currently runs with a trusted stack with no entries (trusted stack frames are required only on cross-compartment calls, so a zero-deep stack can hold one compartment).
This will need changing to be one deep.
After boot, this is used as the register-save area for the idle thread.
This never makes cross-compartment calls, so the loader could potentially adjust its trusted stack pointer to make it act as if it has no frames remaining for some defence in depth (though compromising the idle thread, which simply does `wfi` in a loop, is probably not a sensible thing to worry about).

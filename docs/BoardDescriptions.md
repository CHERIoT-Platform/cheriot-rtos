CHERIoT RTOS Board Descriptions
===============================

CHERIoT RTOS is intended to run on any core that implements the CHERIoT ISA.
Our initial prototype was a modification of the Flute core, our initial production implementation is based on Ibex, and we also use a software simulator generated from the Sail formal model.
The formal model is fairly standard but the cores can run in simulation, FPGA, or as ASICs with different on-chip peripherals, address space layouts, and so on.

To allow software to be portable across these and other implementations, we use a board description file.
This is a JSON document that contains a single object describing the board.
The parser supports a small superset of JSON, in particular it permits hex numbers as well as decimal, which is particularly useful for memory addresses.

The [`boards`](../sdk/boards) directory contains some existing examples.

Memory layout
-------------

Our security guarantees for the shared heap depend the mechanism that allows the allocator to mark memory as quarantined.
Any pointer to memory in this region is subject to a check (by the hardware) on load: if it points to deallocated memory then it will be invalidated on load.
This mechanism is necessary only for memory that can be reused by different trust domains during a single boot.
Memory used to hold globals and code does not require it and so an implementation may save some hardware and power costs by supporting these temporal safety features for only a subset of memory.
As such, we require a range of memory that is used for static code and data ('instruction memory') that is not required to support this mechanism and an additional range that *must* support this for use as the shared heap ('heap memory').
Implementations may choose not to make this separation and provide a single memory region.
At some point, we expect to further separate the mutable and immutable portions of instruction memory so that we can support execute in place.

Instruction memory is described by the `instruction_memory` property.
This must be an object with a `start` and `end` property, each of which is an address.

The region available for the heap is described in the `heap` property.
This must describe the region over which the load filter is defined.
If its `start` property is omitted, then it is assumed to start in the same place as instruction memory.

The Sail board description has a simple layout:

```json
    "instruction_memory": {
        "start": 0x80000000,
        "end": 0x80040000
    },
    "heap": {
        "end": 0x80040000
    },
```

This starts instruction memory at the default RISC-V memory address and has a single 256 KiB region that is used for both kinds of memory.

MMIO Devices
------------

Each memory-mapped I/O device is listed as an object within the `devices` field.
The name of the field is the name of the device and must be an object that contains a `start` and either a `length` or `end` property that, between them, describe the memory range for the device.
Software can then use the `MMIO_CAPABILITY` macro with the name of the device to get a capability to that device's MMIO range and can use `#if DEVICE_EXISTS(device_name)` to conditionally compile code if that device exists.

The Sail model is very simple and so provides only three devices:

```json
    "devices": {
        "clint": {
            "start": 0x2000000,
            "length": 0x10000
        },
        "uart": {
            "start": 0x10000000,
            "end":   0x10000100
        },
        "shadow" : {
            "start" : 0x83000000,
            "end"   : 0x83001000
        }
    },
```

This describes the core-local interrupt controller (`clint`), a UART, and the shadow memory used for the temporal safety mechanism (`shadow`).
The UART, for example, is referred to in source using `MMIO_CAPABILITY(struct Uart, uart)`, which evaluates to a `volatile struct Uart *`, giving a capability to this device.

Interrupts
----------

External interrupts should be defined in an array in the `interrupts` property.
Each element has a `name`, a `number` and a `priority`.
The name is used to refer to this in software and must be a valid C identifier.
The number is the interrupt number.
The priority is the priority with which this interrupt will be configured in the interrupt controller.

Hardware features
-----------------

Some properties define base parts of hardware support.
The `revoker` property is either absent (no temporal safety support), `"software"` (revocation is implemented via a software sweep) or `"hardware"` (there is a hardware revoker).
We expect this to be `"hardware"` on all real implementations, the software revoker exists primarily for the Sail model and the no temporal safety mode only for benchmarking the overhead of revocation.

If the `stack_high_water_mark` property is set to true, then we assume the CPU provides CSRs for tracking stack usage.
This property is primarily present for benchmarking as all of our targets currently implement this feature.

Clock configuration
-------------------

The clock rate is configured by two properties.
The `timer_hz` field is the number of timer increments per second, typically the clock speed of the chip (the RISC-V timer is defined in terms of cycles).
The `tickrate_hz` specifies how many scheduler ticks should happen per second.
See the [timeout documentation](Timeout.md) for more discussion about ticks.

Conditional compilation
-----------------------

The `defines` property specifies any pre-defined macros that should be set when building for this board.
The `driver_includes` property contains an array (in priority order) of include directories that should be added for this target.
Each of the paths in `driver_includes` is, by default, relative to the location of the board file (which allows the board file and drivers to be distributed together).
Optionally, it may include the string `$(sdk)`, which will be replaced by the full path of the SDK directory.
For example, `"$(sdk)/include/platform/generic-riscv"` will expand to the generic RISC-V directory in the SDK.

The driver headers use `#include_next` to include more generic files and so it is important to list the directories containing your overrides first.

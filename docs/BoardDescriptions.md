CHERIoT RTOS Board Descriptions
===============================

CHERIoT RTOS is intended to run on any core that implements the CHERIoT ISA.
Our initial prototype was a modification of the Flute core, our initial production implementation is based on Ibex, and we also use a software simulator generated from the Sail formal model.
The formal model is fairly standard but the cores can run in simulation, FPGA, or as ASICs with different on-chip peripherals, address space layouts, and so on.

To allow software to be portable across these and other implementations, we use a board description file.
This is a JSON document that contains a single object describing the board.
The parser supports a small superset of JSON, in particular it permits hex numbers as well as decimal, which is particularly useful for memory addresses.

Memory layout
-------------

Instruction memory is described by the `instruction_memory` property.
This must be an object with a `start` and `end` property, each of which is an address.

The region available for the heap is described in the `heap` property.
This must describe the region over which the load filter is defined.
If its `start` property is omitted, then it is assumed to start in the same place as instruction memory.

MMIO Devices
------------

Each memory-mapped I/O device is listed as an object within the `devices` field.
The name of the field is the name of the device and must be an object that contains a `start` and either a `length` or `end` property that, between them, describe the memory range for the device.
Software can then use the `MMIO_CAPABILITY` macro with the name of the device to get a capability to that device's MMIO range and can use `#if DEVICE_EXISTS(device_name)` to conditionally compile code if that device exists.

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

Conditional compilation
-----------------------

The `defines` property specifies any pre-defined macros that should be set when building for this board.
The `driver_includes` property contains an array (in priority order) of include directories that should be added for this target.
Each of the paths in `driver_includes` is, by default, relative to the location of the board file (which allows the board file and drivers to be distributed together).
Optionally, it may include the string `$(sdk)`, which will be replaced by the full path of the SDK directory.
For example, `"$(sdk)/include/platform/generic-riscv"` will expand to the generic RISC-V directory in the SDK.

The driver headers use `#include_next` to include more generic files and so it is important to list the directories containing your overrides first.

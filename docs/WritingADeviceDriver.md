Writing a CHERIoT Device Driver
===============================

CHERIoT aims to be small and easy to customize.
It does not have a generic device driver interface but it does have a number of tools that make it possible to write modular device drivers.

What is a device?
-----------------

From the perspective of the CPU, a device is something that you communicate with via a memory-mapped I/O interface, which may (optionally) generate interrupts.
There are several devices that the core parts of the RTOS interact with:

 - The UART, which is used for writing debug output during development.
 - The core-local interrupt controller, which is used for managing timer interrupts.
 - The platform interrupt controller, which is used for managing external interrupts.
 - The revoker, which scans memory for dangling capabilities (pointers) and invalidates them.

Specifying a device's locations
-------------------------------

Devices are specified in the [board description file](BoardDescriptions.md).
The two relevant parts are the `devices` node, which specifies the memory-mapped I/O devices and the `interrupts` section that describes how external interrupts should be configured.
For example, our initial FPGA prototyping platform had sections like this describing its Ethernet device:

```json
    "devices" : {
        "ethernet" : {
            "start" : 0x98000000,
            "length": 0x204
        },
        ...
    },
    "interrupts": [
        {
            "name": "Ethernet",
            "number": 16,
            "priority": 3
        }
    ],
```

The first part says that the ethernet device's MMIO space is 0x204 bytes, starting at address 0x98000000.
The second says that interrupt number 16 is used for the ethernet device.

Accessing the memory-mapped I/O region
--------------------------------------

The `MMIO_CAPABILITY` macro is used to get a pointer to memory-mapped I/O devices.
This takes two arguments.
The first is the C/C++ type of the pointer, the second is the name from the board configuration file.
For example, to get a pointer to the memory-mapped I/O space for the ethernet device above, we might do something like:

```c
struct EthernetMMIO
{
    // Control register layout here:
    ...
};

volatile struct EthernetMMIO *ethernet_device()
{
    return MMIO_CAPABILITY(struct EthernetMMIO, ethernet);
}
```

Note that this macro must be used in code, it cannot be used for static initialisation.
The macro expands to a load from the compartment's import table and so there is no point assigning the result to a global: you will get smaller code using it directly.

Now that you have a pointer to a `volatile` object representing the device's MMIO region, you can access its control registers directly.
Any device can be accessed from any compartment in this way, but that access will appear in the linker audit report.

For this device, you will see an entry like this for any compartment that accesses the device:

```json
        {
          "kind": "MMIO",
          "length": 516,
          "start": 2550136832
        },
```

You can then audit whether a firmware image enforces whatever policy you want (for example, no compartment other than a device driver may access the device directly).
Note that the linker reports will always provide the addresses and lengths in decimal, because they are standard JSON.
We support a small number of extensions to JSON in the files that we consume, to improve usability, but don't use these in files that we produce, to improve interoperability.

There is no requirement to expose a device as a single MMIO region.
You may wish to define multiple regions, which can be as small as a single byte, so that you can privilege separate your device driver.

Some devices have a very large control structure.
For example, the platform-local interrupt controller is many KiBs.
We don't define a C structure that covers every single field for this and instead just use `uint32_t` as the type for `MMIO_CAPABILITY`, which lets us treat the space as an array of 32-bit control registers.

Handling interrupts
-------------------

To be able to handle interrupts, you must have a [software capability](SoftwareCapabilities.md) that [authorises access to the interrupt](Interrupts.md).
For the ethernet device that we've been using as an example, you would typically request one with this macro invocation:

```c
DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(ethernetInterruptCapability, Ethernet, true, true);
```

If you wish to share this between multiple compilation units, you can use the separate `DECLARE_` and `DEFINE_` forms (see [`interrupt.h`](../sdk/include/interrupt.h)) but the combined form is normally most convenient.
This macro takes four arguments:

 1. The name that we're going to use to refer to this capability.
    The name `ethernetInterruptCapability` is arbitrary, you can use whatever makes sense to you.
 2. The name of the interrupt, from the board description file (`Ethernet`, in this case).
 3. Whether this capability authorises waiting for this interrupt (this will almost always be `true`).
 4. Whether this capability authorises acknowledging the interrupt so that it can fire again.
    This will almost always be true in device drivers but should generally be true for only one compartment (for each interrupt), whereas multiple compartments may wish to observe interrupts for monitoring.

As with the MMIO capabilities, sealed objects appear in compartment reports.
For example, the above macro expands to this in the final report:

```json
        {
          "contents": "10000101",
          "kind": "SealedObject",
          "sealing_type": {
            "compartment": "sched",
            "key": "InterruptKey",
            "provided_by": "build/cheriot/cheriot/release/example-firmware.scheduler.compartment",
            "symbol": "__export.sealing_type.sched.InterruptKey"
          }
```

The sealing type tells you that this is an interrupt capability (it's sealed with the `InterruptKey` type, provided by the scheduler).
The contents lets you audit what this authorises.
The first two bytes are a 16-bit (little-endian on all currently supported targets) integer containing the interrupt number, so 1000 means 16 (our Ethernet interrupt number).
The next two bytes are boolean values reflecting the last two arguments to the macro, so this authorises both waiting and clearing the macro.
Again, this can form part of your firmware auditing.

### Waiting on an interrupt

Now that you're authorised to handle interrupts, you will need something that you can wait on.
Each interrupt is mapped to a futex word, which can be used with scheduler waiting primitives.
You can get the word associated with an interrupt by passing the authorising capability to the `interrupt_futex_get` function exported by the scheduler:

```c
const uint32_t *ethernetFutex = ethernetFutex = interrupt_futex_get(STATIC_SEALED_VALUE(ethernetInterruptCapability));
```

The `ethernetFutex` pointer is now a read-only capability (attempting to store through it will trap) that contains a number that is incremented every time the ethernet interrupt fires.
You can now query whether any interrupts have fired since you last checked by comparing it against a previous value and you can wait for an interrupt with `futex_wait`, for example:

```c
do
{
    uint32_t last = *ethernetFutex;
    // Handle interrupt here
} while (futex_wait(ethernetFutex, last) == 0);
```

If you want to wait for multiple event sources, you can use the [multiwaiter](../sdk/include/multiwaiter.h) API.
This allows sleeping on multiple kinds of event source so you can, for example, have a single thread that blocks waiting for a message to send from another thread or a message to receive from the device.

### Acknowledging interrupts

If you copy the last example into a real device driver then you might be surprised that the loop runs twice and then stops.
It will run once on start, once when the first interrupt is delivered, and then never again.
This is because external interrupts are not delivered on a particular channel unless the preceding one has been acknowledged.
A more complete version of the loop above looks like this:

```c
do
{
    uint32_t last = *ethernetFutex;
    // Handle interrupt here
    interrupt_complete(STATIC_SEALED_VALUE(ethernetInterruptCapability));
} while ((last != *ethernetFutex) || futex_wait(ethernetFutex, last) == 0);
```

This includes two changes.
The first is the call to `interrupt_complete` once the interrupt has been handled.
This tells the scheduler to mark the interrupt as completed in the interrupt handler.
It is possible that the interrupt will then fire immediately, in which case there's no point trying to sleep.
The second change checks whether the value of the futex word has changed - if it has, then we skip the `futex_wait` call and handle the next interrupt immediately.

Exposing device interfaces
--------------------------

Device drivers can be designed for incorporation into another compartment (as, for example, our revoker drivers), in which case they can simply be provided as code that's linked into a department.
This is the model used for most of the core devices, where each one is provided as a header file that provides a C++ class that implements a concept.
This allows compile-time selection of the right device.

Other devices will want to live in a separate compartment and expose functions to consumers.

Code structure
--------------

Driver code does not *have* to follow any specific structure but there are some tools that may be helpful for making devices reusable.

### Platform includes

Each board description contains a set of include paths.
For example, our Flute prototype platform has this:

```json
    "driver_includes" : [
        "../include/platform/flute",
        "../include/platform/generic-riscv"
    ],
```

These are added *in this order*, which makes it possible for code in the more specialised directories to `#include_next` versions of the files in the more generic versions or to add files that are found in preference to the generic versions.

### Conditional compilation

The `DEVICE_EXISTS` macro can be used with `#if` to conditionally compile code depending on whether the current board provides a definition of the device.
This is keyed on the existence of an MMIO region in the board description file with the specified name.
For example, the ethernet device that we've been using as an example could be protected with:

```c
#if DEVICE_EXISTS(ethernet)
// Driver for the ethernet device here.
#endif
```

Note: This highlights why "ethernet" is not a great name for the device: ideally the name should be specific to the hardware interface, not the high-level functionality, so that you can conditionally compile specific drivers.
We have used a generic name in this tutorial to avoid introducing device-specific complications.

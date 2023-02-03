Compartmentalised hello world
=============================

The [hello world example](../1.hello_world) used a single compartment, which had direct access to the UART device.
In general, it is bad practice to grant direct access to devices to most compartments and so in this example we'll start moving the UART access to a separate compartment.

*Note:* The `debug.hh` interfaces use direct access to the UART, but only in debug builds.
Debug builds are often allowed to weaken security rules in exchange for a better debugging experience.

In this version, [`uart.cc`](uart.cc) exposes a single function that writes a string to the UART.
The code in [`hello.cc`](hello.cc) calls this with a global string and again with an on-stack buffer.

What do we gain from this compartmentalisation?
In an example this simple, not a huge amount (especially given that neither compartment operates in a way that an attacker could influence, since we don't have any external inputs), but the principles here apply to more complex examples.
We can statically audit the firmware image to know that:

 - The uart compartment is the only one that has direct access to the UART device.
 - The hello compartment is the only one that calls the `write` entry point on the UART compartment.

If an attacker compromised the hello compartment then the would still be able to call the `write` function to write arbitrary output to the UART, but they would not be able to read or write any of the control registers of the UART.

This is the kind of isolation that device drivers typically provide in operating systems and our uart compartment is an example of one of the simplest possible device drivers:

 - It provides useful interfaces to the underlying hardware that abstract over its implementation.
 - It protects the hardware interfaces from use by arbitrary software on the system.

Note in addition that we've added one more attribute to the function exposed here: `[[cheri::interrupt_state(disabled)]]`.
This means that the uart compartment will write the entire string to the UART, so two different threads writing to it will not interfere.
This is not ideal for something like a `write` function, which can take a caller-controlled amount of time to complete, because it prevents any other thread from making progress, even ones that don't touch the uart.

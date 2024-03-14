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

## Validating the compartmentalisation model

The goal of this refactoring was to ensure that only the `uart` compartment has access to the UART device.
How do we know if we achieved that?
This is where the [cheriot-audit](https://github.com/CHERIoT-Platform/cheriot-audit) tool comes in.
If you're using the dev container, this is installed in `/cheriot-tools/bin/`, if you've built it somewhere else then replace `/cheriot-tools/bin/` with the correct path.

First, let's use it to see which compartments or libraries have access to the UART:

```
$ /cheriot-tools/bin/cheriot-audit -b ../../sdk/boards/sail.json -j build/cheriot/cheriot/release/hello_compartment.json -q 'data.compartment.compartments_with_mmio_import(data.board.devices.uart)'
["debug", "scheduler"]
```

This uses `-b` to find the board description file and `-j` the report that the linker generated during the build.
This tells us that the `debug` library and the `scheduler` compartment both have direct access to the UART.
The latter is an artefact of how some simulators exit: simulation builds will have the UART exposed to the scheduler.

Note that our `uart` compartment isn't on this list.
It accesses the UART via the `debug` library.
This means that, in addition to the compartments and libraries that have direct access to the UART, we need to determine which compartments have access to the `debug` library.
Let's run that as another query:

```
$ /cheriot-tools/bin/cheriot-audit -b ../../sdk/boards/sail.json -j build/cheriot/cheriot/release/hello_compartment.json -q 'data.compartment.compartments_calling("debug")'
["uart"]
```

Great!
Our compartmentalisation goal has been achieved!

Note that it was *not* in the first version of this example (ooops!).
The `fail-simulator-on-error.h` header adds an error handler that writes debug messages via direct access to the UART.
This was difficult to spot manually, but would show up in auditing easily.
Try uncommenting this line in `hello.cc`:

```c++
//#include <fail-simulator-on-error.h>
```

Rerunning the above query should now show the following output:

```json
["hello", "uart"]
```

Now that we have something that is correct, we'd like to make sure that it *remains* correct.
We do this by writing a Rego module that encapsulates out policy.

First, let's start with a rule that checks access to the MMIO region.

```rego
# For non-simulation platforms, we only allow the debug library to access the
# UART
uart_access_valid {
	data.compartment.mmio_allow_list("uart", {"debug"})
}

# For simulation platforms, we allow the scheduler to access the UART as well
uart_access_valid {
	data.compartment.mmio_allow_list("uart", {"debug", "scheduler"})
	data.board.simulation
}
```

Note that we've specified this rule twice.
Rego rules match if either instance is true.
The expressions within the braces are anded together: all must be true for the rule to be true.
This means that we're happy with either of two cases:

 - The only thing with access to the `uart` device is the `debug` library.
 - The only things with access to the `uart` device are the `debug` library and the `scheduler` compartment *and* we are targeting a simulator.

We now want to add a rule that holds only if this is true and if the only caller of the `debug` library is the `uart` compartment:

```rego
# Check that the UART is accessible only to the authorised libraries and
# compartments and that only the `uart` compartment can call the library that
# has direct access.
valid {
	uart_access_valid
	data.compartment.compartment_allow_list("debug", {"uart"})
}
```

With this, we can now run the tool to check whether the built firmware complies with our policy:

```
$ /cheriot-tools/bin/cheriot-audit -b ../../sdk/boards/sail.json -j build/cheriot/cheriot/release/hello_compartment.json -m policy.rego  -q 'data.hello_compartment.valid'
true
```

If this outputs `true`, it worked!
Try modifying the code and see if it still passes.
Configuring the build with allocator debugging enabled will cause the policy check to fail, so this can even catch cases where you left debugging access to the UART enabled in a production build.

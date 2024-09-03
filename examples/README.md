Examples
========

This directory contains a number of examples to understand the system:

 1. [Hello world](01.hello_world) is a simple example with a single compartment.
 2. [Compartmentalised hello world](02.hello_compartment) splits the first example into two compartments.
 3. [A more secure compartmentalised hello world](03.hello_safe_compartment) protects the UART compartment from the previous example against malicious callers.
 4. [A demonstration of heap temporal safety](04.temporal_safety) shows that use-after-free bugs will deterministically trap.
 5. [A sealing demonstration](05.sealing) shows how the CHERI sealing mechanism can protect opaque types across compartment boundaries.
 6. [A trivial producer-consumer system](06.producer-consumer) shows the basics of inter-thread communication.
 7. [Handling errors in compartments](07.error_handling) shows how to recover from crashes in compartments.
 8. [Memory safety examples](08.memory_safety) demonstrates how the CHERIoT platform enforces memory safety.
 9. [JavaScript](09.javascript) is a simple example embedding a JavaScript VM in a compartment.
10. [A bogomips estimator](10.bogomips) for comparing target "devices" (mostly simulators).

Each example contains a readme file explaining what it is showing.

Building an example
-------------------

Each example is built with xmake, which first requires a configure step in the terminal:

```sh
$ cd {example_dir}
$ xmake config --sdk={path-to-cheriot-llvm-tools} -P .
```

If you are using the dev container then the `--sdk` flag will be `--sdk=/cheriot-tools/`.

You can pass some additional flags to this step.

 - `-m {debug,release}` will switch between debug and release builds.
 - `--debug-{allocator,scheduler,loader}={y,n}` will enable verbose output for the specified core RTOS component.
 - `--board={board name}` to compile for a target other than the default (the interpreter generated from the formal model).

Next, build the firmware image with:

```sh
$ xmake -P . -v
```

The `-v` (verbose) flag is optional but an xmake bug prevents the objdump output from being generated if it is not specified.
You will then end up with a firmware image and a `.dump` file containing the objdump output in `build/cheriot/cheriot/{debug,release}`.
The penultimate two status lines of the output will be something like:

```
[ 95%]: linking firmware build/cheriot/cheriot/debug/producer-consumer
[ 95%]: Creating firmware dump build/cheriot/cheriot/debug/producer-consumer.dump
```

The first of these tells you the path to the firmware image, the second the location of the dump file that is a (somewhat) human-readable version of the same file.

By default, the examples are built targeting the interpreter that is generated from the Sail formal model of the ISA.
This is the gold model, so anything that runs there should run on any compliant implementation.
This interpreter includes an ELF loader and so can run the examples directly, for example:

```sh
$ cheriot_sim -V ./build/cheriot/cheriot/debug/producer-consumer
```

Where `debug` is the value that you passed to the `-m` flag (`debug` is the default and the path that ends in `producer-consumer` is the path from the build output.

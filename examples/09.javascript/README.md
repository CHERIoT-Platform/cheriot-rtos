JavaScript example
===================

This example shows a simple JavaScript hello world code fragment being interpreted on the device.
The JavaScript is in [`hello.js`](hello.js).
This is a trivial fragment of JavaScript that demonstrates exporting a function to C/C++, importing a function for use by JavaScript, and allocating an array.

For this simple example, the JavaScript bytecode is embedded in the firmware image, though in a more realistic use case it would most likely be fetched from an external source.
If you want to modify the example, then you need to install the JavaScript compiler from the [Microvium](https://github.com/coder-mike/microvium) project:

```
$ npm install microvium
```

By default, `npm` installs things in the current directory, and so this will leave you with a program that can be run like this:

```
$ ./node_modules/.bin/microvium --output-bytes hello.js > bytecode.inc
```

Try modifying the JavaScript code in the example and recompiling.

The Microvium JavaScript interpreter is provided as a shared library and so can be used in multiple compartments.
Each compartment will consume only the memory required for the JavaScript runtime, interpreter stack, and JavaScript heap.

This simple example demonstrates the JavaScript garbage-collected (GC'd) heap in two ways:

 - It allocates an array that is returned from a function and so must live on the GC'd heap
 - In the `print` function, the C++ wrapper coerces values to strings and receives pointers to the GC'd heap.

At the end, we explicitly force the GC to run in its most aggressive mode and report the memory consumption.
You should see some output somewhat like this:

```
Hello, World!
array[0] = 2
array[1] = 4
array[2] = 6
array[3] = 8
array[4] = 10
JavaScript hello compartment: Microvium is using 0x19e bytes of memory, including 0x60 bytes of heap
JavaScript hello compartment: Running GC
JavaScript hello compartment: Microvium is using 0x7e bytes of memory, including 0x14 bytes of heap
JavaScript hello compartment: Peak heap used: 0x60 bytes, peak stack used: 0x28 bytes
```

The first lines are output from JavaScript, the ones prefixed with 'JavaScript hello compartment' are debug log messages.
After running the call, this compartment had used 0x19e (414) bytes of memory for all state associated with JavaScript.
A more complex JavaScript program may require a few KiBs of space, but much of it is gone by the end of a phase of computation as a result of running the garbage collector.
The shared heap on the CHERIoT platform makes it possible to pack several isolated JavaScript VMs on a single small device.
The interpreter itself consumes around 13 KiB of RAM for the code that is shared between all compartments, so 32 KiB of SRAM is sufficient for a dozen or so isolated JavaScript compartments.

Note that the RAM usage of this example can be reduced by calling `mvm_runGC` after processing each argument in the `print` function.

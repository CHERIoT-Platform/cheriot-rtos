Hello World example
===================

This example shows a simple hello-world firmware image, with a single thread and a single compartment.
This uses the debug facilities to write a message directly to the UART.

The [`xmake.lua`](xmake.lua) file contains the build instructions for this example.
This file contains two libraries (one providing `memcpy`, the other `clz`) that can be used by multiple compartments, and a single compartment (beyond the RTOS components).

The code for the compartment is all in [`hello.cc`](hello.cc).
This file first imports the debug facilities:

```c++
using Debug = ConditionalDebug<true, "Hello world compartment">;
```

The `ConditionalDebug` template is intended to allow the first argument to be an expression that may include macro definitions so that debugging can be selectively enabled for subsets of a program at build time.
For now, we're just enabling it unconditionally so that we can use its `log` method to write to the UART.

This will, indirectly, use the `MMIO_CAPABILITY` macro, which imports a memory-mapped I/O device, the target platform's default UART.
Our example compartment will be granted direct access to the UART, which it can then use from its entry point function, `say_hello`.

Note that the definition of this function uses a macro to indicate the compartment that it is in (`"hello"`).
This macro is normally used on function definitions, rather than declarations, and allows the compiler to either export the function when it is defined or insert cross-compartment calls when it is used when compiling code for another compartment.

We also include the SDK header `fail-simulator-on-error.h`. This defines a
simple [compartment error handler](../docs/ErrorHandling.md) that reports any
unexpected errors and ends the simulation with exit code 1 (FAILURE). This is
necessary because the default behaviour would silently exit the thread,
potentially hiding errors.

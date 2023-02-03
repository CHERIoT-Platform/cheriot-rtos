C/C++ extensions for CHERIoT
============================

The CHERIoT platform adds a small number of C/C++ annotations to support the compartment model.

## Attributes for cross-component calls

We introduce several attributes that specify functions that can be exposed from either shared libraries or compartments.

### Compartment entry points

The `cheri_compartment({name})` attribute specifies the name of the compartment that defines a function.
This is used in concert with the `-cheri-compartment=` compiler flag.
This allows the compiler to know whether a particular function (which may be in another compilation unit) is defined in the same compartment as the current compilation unit, allowing direct calls for functions in the same compilation unit and cross-compartment calls for other cases.

This can be used on either definitions or declarations but is most commonly used on declarations.

If a function is defined while compiling a compilation unit belonging to a different compartment then the compiler will raise an error.
This attribute can also be used via the `__cheri_compartment({name})` macro, which allows it to be defined away when targeting other platforms.

### Library entry points

The `cheri_libcall` attribute specifies that this function is provided by a library (shared between compartments).
Libraries may not contain any writeable global variables.
This attribute is implicit for all compiler built-in functions, including `memcpy` and similar freestanding C environment functions.
As with `cheri_compartment()`, this may be used on both definitions and declarations.

This attribute can also be used via the `__cheri_libcall` macro, which allows it to be defined away when targeting other platforms.

### Cross-compartment callback

The `cheri_ccallback` attribute specifies a function that can be used as an entry point by compartments that are passed a function pointer to it.
This attribute must also be used on the type of function pointers that hold cross-compartment invocations.
Any time the address of such a function is taken, the result will be a sealed capability that can be used to invoke the compartment and call this function.

This attribute can also be used via the `__cheri_callback` macro, which allows it to be defined away when targeting other platforms.

## Interrupt state control

The `cheri_interrupt_state` attribute (commonly used as a C++11 / C23 attribute spelled `cheri::interrupt_state`) is applied to functions and takes an argument that is either:

 - `enabled`, to enable interrupts when calling this function.
 - `disabled`, to disable interrupts when calling this function.
 - `inherit`, to not alter the interrupt state when invoking the function.

For most functions, `inherit` is the default.
For cross-compartment calls, `enabled` is the default and `inherit` is not permitted.

The compiler may not inline functions at call sites that would change the interrupt state and will always call them via a sentry capability set up by the loader.
This makes it possible to statically reason about interrupt state in lexical scopes.

The [`cheri.hh`](../sdk/include/cheri.hh) file contains a helper for C++ code to run a lambda with interrupts disabled.

## Importing MMIO access


The `MMIO_CAPABILITY({type}, {name})` macro is used to access memory-mapped I/O devices.
These are specified in the board definition file by the build system.
The `DEVICE_EXISTS({name})` macro can be used to detect whether the current target provides a device with the specified name.
The `type` parameter is the type used to represent the MMIO region.
The macro evaluates to a `volatile {type} *`, so `MMIO_CAPABILITY(struct UART, uart)` will provide a `volatile struct UART *` pointing (and bounded) to the device that the board definition exposes as `uart`.


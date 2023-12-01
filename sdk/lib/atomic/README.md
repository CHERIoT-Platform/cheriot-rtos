Atomic operation support functions
==================================

This directory contains implementations of the atomic support functions.
This exposes a set of dependencies that you can use:

 - `atomic1` provides one-byte atomics, commonly used for atomic booleans used as flags.
 - `atomic2` provides two-byte atomics.
 - `atomic4` provides four-byte atomics, which can be used in conjunction with the scheduler's futex APIs.
 - `atomic8` provides capability-width atomics.
 - `atomic_fixed` brings in all of the above.
 - `atomic` supports arbitrary-sized atomics.

The full atomics library will provide a complete set of atomic functions, at the expense of around 700 bytes of code.

A lot of systems are likely to need only a subset of these operations and so should aim to depend on only the subset that they use.
The variable-width atomics provided by `atomic`, are required by C11 but are rarely used.
The C++11 atomics implementation in CHERIoT RTOS uses an inline lock for large atomic locks and so does not depend on this.

If you wish to aggressively reduce code size then you can use the macros in [`atomic.hh`](atomic.hh) to define an explicit subset of operations.
For example, if you need only the `__atomic_load_1` function required for C++ thread-safe static initialisation, then you can just insert `DEFINE_ATOMIC_LOAD(1, uint8_t)` to define that function.


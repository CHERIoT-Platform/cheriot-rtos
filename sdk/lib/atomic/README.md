Atomic operation support functions
==================================

This library contains implementations of the atomic support functions.
Adding this library directly will provide a complete set of atomic functions, at the expense of around 700 bytes of code.
Each of the individual `.cc` files provides support for different-sized atomics.
For example, `atomic4.cc` provides support for 4-byte (32-bit) atomic operations.
The `atomicn.cc` file provides support for arbitrary-width operations.

A lot of systems are likely to need only a subset of these operations.
You can provide subsets of these in three different ways.
First, you can depend on the `atomic-fixed` target rather than `atomic`.
This excludes the variable-width atomics, which are required by C11 / C++11 but which are rarely used.

If you know that you are using atomics only of a single size, you can define your own library target that specifies a custom subset of the files.
For example, if you want to use 1- and 4-byte atomics, you could write this in your `xmake.lua`:

```lua
library("atomic-custom")
  add_files("atomic1.cc",
            "atomic4.cc")
```

Now you can depend on this target to get only the required functions.

Finally, you can use the macros in [`atomic.hh`](atomic.hh) to define an explicit subset of operations.
For example, if you need only the `__atomic_load_1` function required for C++ thread-safe static initialisation, then you can just insert `DEFINE_ATOMIC_LOAD(1, uint8_t)` to define that function.


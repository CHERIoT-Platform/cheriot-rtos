Math library
============

This library provides implementations for a subset of functions normally provided by the C math library / libm, and which are expected by many LLVM intrinsics.

The library exposes the following targets which your project can depend on:

- `math32` provides operations on 32-bit `float` types.
- `math64` provides operations on 64-bit `double` types.
- `math` provides all of the above.

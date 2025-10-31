Soft-float support library
==========================

This library provides a software floating-point implementation for CPUs that lack one.

The soft-float library provides several targets that you can add.
Most of the time you can use one of four helper targets:

 - `softfloat32` provides support for all operations on scalar `float`s except `pow`.
 - `softfloat64` provides support for all operations on scalar `double`s except `pow`.
 - `softfloat` provides support for all operations on scalar `float`s and `double`s except `pow`.
   This includes conversion between 32-bit and 64-bit floating-point values.
 - `softfloatall` provides support for all floating-point operations.

Each of these composite targets is implemented as depending on the other smaller libraries so don't worry about code duplication.
If one compartment depends on `softfloat32` and another depends on `softfloat` then you will get a single copy of the functions in `softfloat32`.

If you need to reduce size, you can instead adopt some of the component libraries
Each of the basic operations is provided by a separate library.

 - The `softfloat{32,64}{add,sub,mul,div,neg}` libraries each provides a single operation (add, subtract, multiply, divide, unary negate).
 - The `softfloat{32,64}compare` libraries provide the various comparisons (equality, ordered comparisons).
 - The `softfloat{32,64}convert` libraries provide conversion functions for converting between integer and 32/64-bit floating-point types.
 - The `softfloat{32,64}pow` libraries provide functions for raising a floating-point value to a power.
 - The `softfloat{32,64}complex` libraries provide the additional functions required for multiplying and dividing complex numbers.

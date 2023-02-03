Optional libraries
==================

This directory contains optional libraries that can be included in firmware images.
Each directory can be included directly in xmake to add the entire library, some of the source files can also be added separately for small images that do not need an entire library.

This collection currently includes:

 - [atomic](atomic/) provides atomic support functions.
 - [crt](crt/) provides C runtime functions that the compiler may emit.
 - [cxxrt](cxxrt/) provides a minimal C++ runtime (no exceptions or RTTI support).
 - [freestanding](freestanding/) provides a minimal free-standing C implementation.
 - [stdio](stdio/) provides a *very* limited subset of `stdio.h` for debugging.
 - [string](string/) provides `string.h` functions.
 - [thread_pool](thread_pool) provides a simple thread pool that other threads can dispatch work to for asynchronous execution.
 - [microvium](microvium/) builds the [microvium](https://github.com/coder-mike/microvium) JavaScript VM to provide an on-device JavaScript interpreter.


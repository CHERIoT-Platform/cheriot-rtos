Optional libraries
==================

This directory contains optional libraries that can be included in firmware images.
Each directory can be included directly in xmake to add the entire library, some of the source files can also be added separately for small images that do not need an entire library.

This collection currently includes:

 - [atomic](atomic/) provides atomic support functions.
 - [compartment_helpers](compartment_helpers/) contains helpers for checking / ensuring that pointers are valid.
 - [crt](crt/) provides C runtime functions that the compiler may emit.
 - [cxxrt](cxxrt/) provides a minimal C++ runtime (no exceptions or RTTI support).
 - [debug](debug/) contains functions to support the debug logging APIs.
 - [event_group](event_group/) contains a FreeRTOS-like event-group API.
 - [freestanding](freestanding/) provides a minimal free-standing C implementation.
 - [locks](locks/) contains functions for various kinds of lock.
 - [microvium](microvium/) builds the [microvium](https://github.com/coder-mike/microvium) JavaScript VM to provide an on-device JavaScript interpreter.
 - [queue](queue/) contains functions for message queues.
 - [stdio](stdio/) provides a *very* limited subset of `stdio.h` for debugging.
 - [string](string/) provides `string.h` functions.
 - [thread_pool](thread_pool) provides a simple thread pool that other threads can dispatch work to for asynchronous execution.
 - [unwind_error_handler](unwind_error_handler) provides an error handler that unwinds the stack.



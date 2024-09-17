Unwind error handler library
----------------------------

This library provides an error handler that unwinds using the APIs in [`unwind.h`](../../include/unwind.h).

This error handler can work even in cases of stack overflow.

Note: Unlike most libraries, this is *not* built as a shared library.
Error handlers must be part of the compartment that invokes them.
As a result, this must be added as a dependency of each compartment that wishes to use it, rather than as a dependency of the firmware target.

Minimal standard output
=======================

This directory contains a *very* minimal subset of `<stdio.h>`.
The `printf` implementation is not fully standards compliant and requires direct access to the UART and so should not be used outside of debug builds.
For C++ code, please consider using the logging facilities in `debug.hh` instead.

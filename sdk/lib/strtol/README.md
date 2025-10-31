String to Number Conversion
===========================

This library contains an implementation of `strtoul` and `strtol`.

In particular, it contains FreeBSD's sys/libkern/strtol.c and sys/libkern/strtoul.c as of commit 856d3167995a5c9e1fc69a69ad393ab9e552c420.
These files are exempt from the CHERIoT RTOS formatting rules for easy comparison with upstream.

Minor changes have been made:
  - Of #include directives
  - Compile as C++ for `const_cast<>` rather than `DECONST`.

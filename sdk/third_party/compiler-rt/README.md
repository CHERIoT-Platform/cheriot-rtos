Soft-float support library
==========================

Files here are copied from compiler-rt, unmodified.

When updating, they should be overridden and the git hash in this file updated.

Ideally we'd use a submodule for this, but unfortunately git doesn't let you have a submodule that points to a single sudirectory in a remote and so we'd have to have a 7.5 GiB LLVM clone to get well under 1 MiB of files.

LLVM git revision: b9977ea5f8abca95f854b62ddd373b031b1dc933

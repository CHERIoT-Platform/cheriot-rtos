Freestanding C++ implementation
===============================

Files here are copied from libc++, unmodified.

When updating, they should be overridden and the git hash in this file updated.

Ideally we'd use a submodule for this, but unfortunately git doesn't let you have a submodule that points to a single sudirectory in a remote and so we'd have to have a 7.5 GiB LLVM clone to get well under 1 MiB of files.

LLVM git revision: 7245208df70403e1f7189f3be7b57ca934b1cbb2

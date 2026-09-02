The files here are copied from `freebsd/lib/msun`.

They should be copied unmodified, but some files may require a single replacement: `^static volatile/static const volatile`, as libraries cannot have mutable globals. These constants are marked volatile to ensure particular fenv behaviour at runtime.

When adding or updating source files, please either copy new files from the commit marked below, or ensure all files are updated to the same revision, and update the commit here.

FreeBSD git revision: 71e72c9e91c4b8007a4292e09669e8b549c29e97

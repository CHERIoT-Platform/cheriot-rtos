// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef _ERRNO_H_
#define _ERRNO_H_

#define ECOMPARTMENTFAIL 1 // Compartment failed
#define EPERM 2            // Operation not permitted.
#define ENOENT 3           // No such file or directory.
#define ESRCH 4            // No such process.
#define EINTR 5            // Interrupted function.
#define EIO 6              // I/O error.
#define ENXIO 7            // No such device or address.
#define E2BIG 8            // Argument list too long.
#define ENOEXEC 9          // Executable file format error.
#define EBADF 10           // Bad file descriptor.
#define ECHILD 11          // No child processes.
#define EAGAIN 12          // Resource unavailable, try again.
#define ENOMEM 13          // Not enough space.
#define EACCES 14          // Permission denied.
#define EFAULT 15          // Bad address.
#define EBUSY 16           // Device or resource busy.
#define EEXIST 17          // File exists.
#define EXDEV 18           // Cross-device link.
#define ENODEV 19          // No such device.
#define ENOTDIR 20         // Not a directory or a symbolic link to a directory.
#define EISDIR 21          // Is a directory.
#define EINVAL 22          // Invalid argument.
#define ENFILE 23          // Too many files open in system.
#define EMFILE 24          // File descriptor value too large.
#define ENOTTY 25          // Inappropriate I/O control operation.
#define ETXTBSY 26         // Text file busy.
#define EFBIG 27           // File too large.
#define ENOSPC 28          // No space left on device.
#define ESPIPE 29          // Invalid seek.
#define EROFS 30           // Read-only file system.
#define EMLINK 31          // Too many links.
#define EPIPE 32           // Broken pipe.
#define EDOM 33            // Math arg out of domain of func.
#define ERANGE 34          // Result too large.
#define ENAMETOOLONG 36    // Filename too long.
#define ENOLCK 37          // No locks available.
#define ENOSYS 38          // Functionality not supported.
#define ENOTEMPTY 39       // Directory not empty.
#define ELOOP 40           // Too many levels of symbolic links.
#define ENOMSG 42          // No message of the desired type.
#define EIDRM 43           // Identifier removed.
#define EDEADLK 45         // Resource deadlock would occur.
#define EUNATCH 49         // Protocol driver not attached.
#define EBADE 52           // Invalid exchange.
#define ENOSTR 60          // Not a STREAM.
#define ENODATA 61         // No data available.
#define ETIME 62           // Timer expired.
#define ENOSR 63           // No STREAM resources.
#define ENOLINK 67         // Reserved.
#define EPROTO 71          // Protocol error.
#define EMULTIHOP 72       // Reserved.
#define EBADMSG 74         // Bad message.
#define EFTYPE 79          // Inappropriate file type or format.
#define EILSEQ 84          // Illegal byte sequence.
#define ENOTSOCK 88        // Not a socket.
#define EDESTADDRREQ 89    // Destination address required.
#define EMSGSIZE 90        // Message too large.
#define EPROTOTYPE 91      // Protocol wrong type for socket.
#define ENOPROTOOPT 92     // Protocol not available.
#define EPROTONOSUPPORT 93 // Protocol not supported.
#define EOPNOTSUPP 95      // Operation not supported on socket.
#define EAFNOSUPPORT 97    // Address family not supported.
#define EADDRINUSE 98      // Address in use.
#define EADDRNOTAVAIL 99   // Address not available.
#define ENETDOWN 100       // Network is down.
#define ENETUNREACH 101    // Network unreachable.
#define ENETRESET 102      // Connection aborted by network.
#define ECONNABORTED 103   // Connection aborted.
#define ECONNRESET 104     // Connection reset.
#define ENOBUFS 105        // No buffer space available.
#define EISCONN 106        // Socket is connected.
#define ENOTCONN 107       // The socket is not connected.
#define ETIMEDOUT 110      // Connection timed out.
#define ECONNREFUSED 111   // Connection refused.
#define EHOSTUNREACH 113   // Host is unreachable.
#define EALREADY 114       // Connection already in progress.
#define EINPROGRESS 115    // Operation in progress.
#define ESTALE 116         // Reserved.
#define EDQUOT 122         // Reserved.
#define ENOMEDIUM 123      // No medium inserted.
#define ECANCELED 125      // Operation canceled.
#define EOWNERDEAD 130     // Previous owner died.
#define ENOTRECOVERABLE 131 // State not recoverable.
#define EOVERFLOW 139       // Value too large to be stored in data type.
#define ENOTENOUGHSTACK 140 // Insufficient stack space for cross-compartment call.
#define EWOULDBLOCK EAGAIN  // Operation would block.
#define ENOTSUP EOPNOTSUPP  // Not supported.
#define __ELASTERROR 2000   // Users can add values starting here.

#endif // _ERRNO_H_

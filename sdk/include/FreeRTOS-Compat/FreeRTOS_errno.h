// Copyright SCI Semiconductor and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <errno.h>

/**
 * This header defines errno values used by FreeRTOS+TCP.
 *
 * In the FreeRTOS core tree, this is included in `projdefs.h`.
 *
 * We modified it here to use the same errno codes are the CHERIoT core when
 * possible.
 */

#define pdFREERTOS_ERRNO_NONE 0                  // No errors
#define pdFREERTOS_ERRNO_ENOENT ENOENT           // No such file or directory
#define pdFREERTOS_ERRNO_EINTR EINTR             // Interrupted system call
#define pdFREERTOS_ERRNO_EIO EIO                 // I/O error
#define pdFREERTOS_ERRNO_ENXIO ENXIO             // No such device or address
#define pdFREERTOS_ERRNO_EBADF EBADF             // Bad file number
#define pdFREERTOS_ERRNO_EAGAIN EAGAIN           // No more processes
#define pdFREERTOS_ERRNO_EWOULDBLOCK EWOULDBLOCK // Operation would block
#define pdFREERTOS_ERRNO_ENOMEM ENOMEM           // Not enough memory
#define pdFREERTOS_ERRNO_EACCES EACCES           // Permission denied
#define pdFREERTOS_ERRNO_EFAULT EFAULT           // Bad address
#define pdFREERTOS_ERRNO_EBUSY EBUSY             // Mount device busy
#define pdFREERTOS_ERRNO_EEXIST EEXIST           // File exists
#define pdFREERTOS_ERRNO_EXDEV EXDEV             // Cross-device link
#define pdFREERTOS_ERRNO_ENODEV ENODEV           // No such device
#define pdFREERTOS_ERRNO_ENOTDIR ENOTDIR         // Not a directory
#define pdFREERTOS_ERRNO_EISDIR EISDIR           // Is a directory
#define pdFREERTOS_ERRNO_EINVAL EINVAL           // Invalid argument
#define pdFREERTOS_ERRNO_ENOSPC ENOSPC           // No space left on device
#define pdFREERTOS_ERRNO_ESPIPE ESPIPE           // Illegal seek
#define pdFREERTOS_ERRNO_EROFS EROFS             // Read only file system
#define pdFREERTOS_ERRNO_EUNATCH EUNATCH         // Protocol driver not attached
#define pdFREERTOS_ERRNO_EBADE EBADE             // Invalid exchange
#define pdFREERTOS_ERRNO_EFTYPE EFTYPE           // Inappropriate file type or format
#define pdFREERTOS_ERRNO_ENOTEMPTY ENOTEMPTY     // Directory not empty
#define pdFREERTOS_ERRNO_ENAMETOOLONG ENAMETOOLONG // File or path name too long
#define pdFREERTOS_ERRNO_EOPNOTSUPP EOPNOTSUPP   // Operation not supported on transport endpoint
#define pdFREERTOS_ERRNO_EAFNOSUPPORT EAFNOSUPPORT // Address family not supported by protocol
#define pdFREERTOS_ERRNO_ENOBUFS ENOBUFS         // No buffer space available
#define pdFREERTOS_ERRNO_ENOPROTOOPT ENOPROTOOPT // Protocol not available
#define pdFREERTOS_ERRNO_EADDRINUSE EADDRINUSE   // Address already in use
#define pdFREERTOS_ERRNO_ETIMEDOUT ETIMEDOUT     // Connection timed out
#define pdFREERTOS_ERRNO_EINPROGRESS EINPROGRESS // Connection already in progress
#define pdFREERTOS_ERRNO_EALREADY EALREADY       // Socket already connected
#define pdFREERTOS_ERRNO_EADDRNOTAVAIL EADDRNOTAVAIL // Address not available
#define pdFREERTOS_ERRNO_EISCONN EISCONN         // Socket is already connected
#define pdFREERTOS_ERRNO_ENOTCONN ENOTCONN       // Socket is not connected
#define pdFREERTOS_ERRNO_ENOMEDIUM ENOMEDIUM     // No medium inserted
#define pdFREERTOS_ERRNO_EILSEQ EILSEQ           // An invalid UTF-16 sequence was encountered
#define pdFREERTOS_ERRNO_ECANCELED ECANCELED     // Operation canceled

/**
 * These errno codes are non-standard, assign them a code outside our errno
 * range.
 */

#define pdFREERTOS_ERRNO_ENMFILE (__ELASTERROR + 1) // No more files

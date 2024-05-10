// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef __STDIO_H__
#define __STDIO_H__

#include <cdefs.h>
#include <compartment-macros.h>
#include <stdarg.h>
#include <stddef.h>

#define PRT_MAX_SIZE (0x80)
#define EOF (-1)

__BEGIN_DECLS

/**
 * This is a very simple implementation of a subset of stdio and supports only
 * UARTs.  The Uart type is often a C++ template type and so we can't forward
 * declare it in a C header and so we use a volatile void* instead, which can
 * be cast to the correct type inside the library.
 */
typedef volatile void FILE;

#if DEVICE_EXISTS(uart0)
#	define stdout MMIO_CAPABILITY(void, uart0)
#	define stdin MMIO_CAPABILITY(void, uart0)
#elif DEVICE_EXISTS(uart)
#	define stdout MMIO_CAPABILITY(void, uart)
#	define stdin MMIO_CAPABILITY(void uart)
#endif

#if DEVICE_EXISTS(uart1)
#	define stderr MMIO_CAPABILITY(void, uart1)
#elif defined(stdout)
#	define stderr stdout
#endif

int __cheri_libcall vfprintf(FILE *stream, const char *fmt, va_list ap);

static inline int fprintf(FILE *stream, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	int ret = vfprintf(stream, format, ap);
	va_end(ap);
	return ret;
}

static inline int printf(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	int ret = vfprintf(stdout, format, ap);
	va_end(ap);
	return ret;
}

int __cheri_libcall snprintf(char *str, size_t size, const char *format, ...);
int __cheri_libcall vsnprintf(const char *str,
                              size_t      size,
                              const char *format,
                              va_list     ap);
__END_DECLS

#endif /* !__STDIO_H__ */

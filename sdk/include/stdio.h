// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef __STDIO_H__
#define __STDIO_H__

#include <cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define PRT_MAX_SIZE (0x80)
#define EOF (-1)

__BEGIN_DECLS
int __cheri_libcall printf(const char *fmt, ...);

#define name_printf(fmt, ...)                                                  \
	printf(__XSTRING(__CHERI_COMPARTMENT__) ": " fmt, ##__VA_ARGS__)

int __cheri_libcall snprintf(char *str, size_t size, const char *format, ...);
int __cheri_libcall vsnprintf(char *str, size_t size, const char *format, va_list ap); 
__END_DECLS

#endif /* !__STDIO_H__ */

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef _STRINGS_H_
#define _STRINGS_H_

#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS
size_t __cheri_libcall clz(uint32_t x);
size_t __cheri_libcall ctz(uint32_t x);
__END_DECLS

#endif // _STRINGS_H_

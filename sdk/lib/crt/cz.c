// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>

size_t __noinline __cheri_libcall clz(uint32_t x)
{
	return __builtin_clz(x);
}

size_t __noinline __cheri_libcall ctz(uint32_t x)
{
	x &= -x;
	return 32U - (clz(x) + 1);
}

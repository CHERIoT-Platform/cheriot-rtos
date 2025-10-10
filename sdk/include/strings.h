// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef _STRINGS_H_
#define _STRINGS_H_

#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS
#if defined(__riscv_zbb)
// If the the bitmanipulation extension is available then use the clang builtins
// as these will compile to a single instruction.
[[clang::always_inline]] size_t clz(uint32_t x)
{
	return __builtin_clz(x);
}
[[clang::always_inline]] size_t ctz(uint32_t x)
{
	return __builtin_ctz(x);
}
#else
// otherwise call the functions in arith64.c (saves space vs expanded builtin)
int __cheri_libcall __ctzsi2(uint32_t a) __asm__("__ctzsi2");
int __cheri_libcall __clzsi2(uint32_t a) __asm__("__clzsi2");
[[clang::always_inline]] size_t clz(uint32_t x)
{
	return __clzsi2(x);
}
[[clang::always_inline]] size_t ctz(uint32_t x)
{
	return __ctzsi2(x);
}
#endif
__END_DECLS

#endif // _STRINGS_H_

// SPDX-License-Identifier: Unlicense
// See arith64.c

#include "cdefs.h"
#include <compartment-macros.h>
#include <stdint.h>

#define arith64_u64 uint64_t
#define arith64_s64 int64_t
#define arith64_u32 uint32_t
#define arith64_s32 int32_t

arith64_s64 __cheri_libcall __ashldi3(arith64_s64 a,
                                      int         b) __asm__("__ashldi3");
arith64_s64 __cheri_libcall __absvdi2(arith64_s64 a) __asm__("__absvdi2");
arith64_s64 __cheri_libcall __ashldi3(arith64_s64 a,
                                      int         b) __asm__("__ashldi3");
arith64_s64 __cheri_libcall __ashrdi3(arith64_s64 a,
                                      int         b) __asm__("__ashrdi3");

#if !defined(__riscv_zbb)
// These should use dedicated instructions when B extension is enabled
int __cheri_libcall __clzsi2(arith64_u32 a) __asm__("__clzsi2");
int __cheri_libcall __clzdi2(arith64_u64 a) __asm__("__clzdi2");
int __cheri_libcall __ctzsi2(arith64_u32 a) __asm__("__ctzsi2");
int __cheri_libcall __ctzdi2(arith64_u64 a) __asm__("__ctzdi2");
int __cheri_libcall __popcountsi2(arith64_u32 a) __asm__("__popcountsi2");
int __cheri_libcall __popcountdi2(arith64_u64 a) __asm__("__popcountdi2");
#endif

arith64_u64 __cheri_libcall __divmoddi4(arith64_u64  a,
                                        arith64_u64  b,
                                        arith64_u64 *c) __asm__("__divmoddi4");
arith64_s64 __cheri_libcall __divdi3(arith64_s64 a,
                                     arith64_s64 b) __asm__("__divdi3");
int __cheri_libcall         __ffsdi2(arith64_u64 a) __asm__("__ffsdi2");
arith64_u64 __cheri_libcall __lshrdi3(arith64_u64 a,
                                      int         b) __asm__("__lshrdi3");
arith64_s64 __cheri_libcall __moddi3(arith64_s64 a,
                                     arith64_s64 b) __asm__("__moddi3");
arith64_u64 __cheri_libcall __udivdi3(arith64_u64 a,
                                      arith64_u64 b) __asm__("__udivdi3");
arith64_u64 __cheri_libcall __umoddi3(arith64_u64 a,
                                      arith64_u64 b) __asm__("__umoddi3");
arith64_s64 __cheri_libcall __multi3(arith64_s64 a,
                                     arith64_s64 b) __asm__("__multi3");

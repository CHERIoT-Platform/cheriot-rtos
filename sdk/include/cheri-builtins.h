// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#ifndef _CHERI_BUILTINS_
#define _CHERI_BUILTINS_

#define CHERI_PERM_GLOBAL (1U << 0)
#define CHERI_PERM_LOAD_GLOBAL (1U << 1)
#define CHERI_PERM_STORE (1U << 2)
#define CHERI_PERM_LOAD_MUTABLE (1U << 3)
#define CHERI_PERM_STORE_LOCAL (1U << 4)
#define CHERI_PERM_LOAD (1U << 5)
#define CHERI_PERM_LOAD_STORE_CAP (1U << 6)
#define CHERI_PERM_ACCESS_SYS (1U << 7)
#define CHERI_PERM_EXECUTE (1U << 8)
#define CHERI_PERM_UNSEAL (1U << 9)
#define CHERI_PERM_SEAL (1U << 10)
#define CHERI_PERM_USER0 (1U << 11)

#define CHERI_OTYPE_BITS 3

#ifdef __cplusplus
#error Do not use this file from C++.
#endif
#ifndef __ASSEMBLER__

#	include <stddef.h>
#	include <stdint.h>

#	define cr_read(name)                                                      \
		({                                                                     \
			void *val;                                                         \
			__asm __volatile("cmove %0, " #name : "=C"(val));                  \
			val;                                                               \
		})

#	define cr_write(name, val)                                                \
		({ __asm __volatile("cmove " #name ", %0" ::"C"(val)); })

static inline void mem_cpy64(volatile uint64_t *dst, volatile uint64_t *p)
{
	volatile void **_dst = (volatile void **)dst;
	volatile void **_p   = (volatile void **)p;

	*_dst = *_p;
}

#	define cgetlen(foo) __builtin_cheri_length_get(foo)
#	define cgetperms(foo) __builtin_cheri_perms_get(foo)
#	define cgettype(foo) __builtin_cheri_type_get(foo)
#	define cgettag(foo) __builtin_cheri_tag_get(foo)
#	define cgetoffset(foo) __builtin_cheri_offset_get(foo)
#	define csetoffset(a, b) __builtin_cheri_offset_set((a), (b))
#	define cincoffset(a, b) __builtin_cheri_offset_increment((a), (b))
#	define cgetaddr(a) __builtin_cheri_address_get(a)
#	define csetaddr(a, b) __builtin_cheri_address_set((a), (b))
#	define cgetbase(foo) __builtin_cheri_base_get(foo)
#	define candperms(a, b) __builtin_cheri_perms_and((a), (b))
#	define cseal(a, b) __builtin_cheri_seal((a), (b))
#	ifdef FLUTE
#		define cunseal(a, b)                                                  \
			({                                                                 \
				__auto_type __a   = (a);                                       \
				__auto_type __b   = (b);                                       \
				__auto_type __ret = __builtin_cheri_tag_clear(__a);            \
				if (__builtin_cheri_tag_get(__a) &&                            \
					__builtin_cheri_tag_get(__b) &&                            \
					__builtin_cheri_type_get(__a) &&                           \
					!__builtin_cheri_type_get(__b))                            \
				{                                                              \
					__auto_type __type = __builtin_cheri_type_get(__a);        \
					__auto_type __base = __builtin_cheri_base_get(__b);        \
					if ((__type >= __base) &&                                  \
						(__type < (__base + __builtin_cheri_length_get(__b)))) \
					{                                                          \
						__ret = __builtin_cheri_unseal((a), (b));              \
					}                                                          \
				}                                                              \
				__ret;                                                         \
			})
#	else
#		define cunseal(a, b) __builtin_cheri_unseal((a), (b))
#	endif
#	define csetbounds(a, b) __builtin_cheri_bounds_set((a), (b))
#	define csetboundsext(a, b) __builtin_cheri_bounds_set_exact((a), (b))
#	define ccheckperms(a, b) __builtin_cheri_perms_check((a), (b))
#	define cchecktype(a, b) __builtin_cheri_type_check((a), (b))
#	define cbuildcap(a, b) __builtin_cheri_cap_build((a), (b))
#	define ccopytype(a, b) __builtin_cheri_cap_type_copy((a), (b))
#	define ccseal(a, b) __builtin_cheri_conditional_seal((a), (b))
#	define cequalexact(a, b) __builtin_cheri_equal_exact((a), (b))

static inline size_t ctestsubset(void *a, void *b)
{
	size_t val;
	__asm volatile("ctestsubset %0, %1, %2 " : "=r"(val) : "C"(a), "C"(b));
	return val;
}

static inline size_t creplenalignmask(size_t len)
{
	size_t ret;
	__asm volatile("cram %0, %1" : "=r"(ret) : "r"(len));
	return ret;
}

static inline size_t croundreplen(size_t len)
{
	size_t ret;
	__asm volatile("crrl %0, %1" : "=r"(ret) : "r"(len));
	return ret;
}

#	define cspecial_write(csr, val)                                           \
		({ __asm __volatile("cspecialw " #csr ", %0" ::"C"(val)); })

#	define cspecial_read(csr)                                                 \
		({                                                                     \
			void *val;                                                         \
			__asm __volatile("cspecialr %0, " #csr : "=C"(val));               \
			val;                                                               \
		})

#endif // __ASSEMBLER__

#endif // _CHERI_BUILTINS_

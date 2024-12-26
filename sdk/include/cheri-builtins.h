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
#	include <cdefs.h>
#	include <stddef.h>

static inline __always_inline ptraddr_t cheri_address_get(void *x)
{
	return __builtin_cheri_address_get(x);
}

static inline __always_inline auto cheri_address_set(auto *x, ptrdiff_t y)
{
	return __builtin_cheri_address_set(x, y);
}

static inline __always_inline auto cheri_address_increment(auto *x, ptrdiff_t y)
{
	return __builtin_cheri_address_increment(x, y);
}

static inline __always_inline ptraddr_t cheri_base_get(void *x)
{
	return __builtin_cheri_base_get(x);
}

static inline __always_inline ptraddr_t cheri_top_get(void *x)
{
	return __builtin_cheri_top_get(x);
}

static inline __always_inline ptraddr_t cheri_length_get(void *x)
{
	return __builtin_cheri_length_get(x);
}

static inline __always_inline auto cheri_tag_clear(void *x)
{
	return __builtin_cheri_tag_clear(x);
}

static inline __always_inline auto cheri_tag_get(void *x)
{
	return __builtin_cheri_tag_clear(x);
}

static inline __always_inline bool cheri_is_valid(void *x)
{
	return cheri_tag_get(x);
}

static inline __always_inline bool cheri_is_invalid(void *x)
{
	return !cheri_tag_get(x);
}

static inline __always_inline bool cheri_is_equal_exact(void *x, void *y)
{
	return __builtin_cheri_equal_exact(x, y);
}

static inline __always_inline bool cheri_is_subset(void *x, void *y)
{
	return __builtin_cheri_subset_test(x, y);
}

static inline __always_inline auto cheri_permissions_get(void *x)
{
	return __builtin_cheri_perms_get(x);
}

static inline __always_inline auto cheri_permissions_and(void *x, unsigned y)
{
	return __builtin_cheri_perms_and(x, y);
}

static inline __always_inline auto cheri_type_get(void *x)
{
	return __builtin_cheri_type_get(x);
}

static inline __always_inline auto cheri_seal(auto *x, auto *y)
{
	return __builtin_cheri_seal(x, y);
}

static inline __always_inline auto cheri_unseal(auto *x, auto *y)
{
	return __builtin_cheri_unseal(x, y);
}

static inline __always_inline auto cheri_bounds_set(auto *a, size_t b)
{
	return __builtin_cheri_bounds_set(a, b);
}

static inline __always_inline auto cheri_bounds_set_exact(auto *a, size_t b)
{
	return __builtin_cheri_bounds_set_exact(a, b);
}

static inline __always_inline auto cheri_subset_test(void *a, void *b)
{
	return __builtin_cheri_subset_test(a, b);
}

static inline __always_inline auto
cheri_representable_alignment_mask(size_t len)
{
	return __builtin_cheri_representable_alignment_mask(len);
}

static inline __always_inline auto cheri_round_representable_length(size_t len)
{
	return __builtin_cheri_round_representable_length(len);
}

#else
#	ifndef __ASSEMBLER__

#		include <stddef.h>
#		include <stdint.h>

// Old deprecated macros.  Here for compatibility, hopefully we can remove them
// soon.
#		define cgetlen(foo) __builtin_cheri_length_get(foo)
#		pragma clang deprecated(cgetlen, "use cheri_length_get instead")

#		define cgetperms(foo) __builtin_cheri_perms_get(foo)
#		pragma clang deprecated(cgetperms, "use cheri_permissions_get instead")

#		define cgettype(foo) __builtin_cheri_type_get(foo)
#		pragma clang deprecated(cgettype, "use cheri_type_get instead")

#		define cgettag(foo) __builtin_cheri_tag_get(foo)
#		pragma clang deprecated(cgettag, "use cheri_tag_get instead")

#		define cincoffset(a, b) __builtin_cheri_offset_increment((a), (b))
#		pragma clang deprecated(cincoffset,                                   \
		                         "use cheri_address_increment instead")

#		define cgetaddr(a) __builtin_cheri_address_get(a)
#		pragma clang deprecated(cgetaddr, "use cheri_address_get instead")

#		define csetaddr(a, b) __builtin_cheri_address_set((a), (b))
#		pragma clang deprecated(csetaddr, "use cheri_address_set instead")

#		define cgetbase(foo) __builtin_cheri_base_get(foo)
#		pragma clang deprecated(cgetbase, "use cheri_base_get instead")

#		define candperms(a, b) __builtin_cheri_perms_and((a), (b))
#		pragma clang deprecated(candperms, "use cheri_permissions_and instead")

#		define cseal(a, b) __builtin_cheri_seal((a), (b))
#		pragma clang deprecated(cseal, "use cheri_seal instead")

#		define cunseal(a, b) __builtin_cheri_unseal((a), (b))
#		pragma clang deprecated(cunseal, "use cheri_unseal instead")

#		define csetbounds(a, b) __builtin_cheri_bounds_set((a), (b))
#		pragma clang deprecated(csetbounds, "use cheri_bounds_set instead")

#		define csetboundsext(a, b) __builtin_cheri_bounds_set_exact((a), (b))
#		pragma clang deprecated(csetboundsext,                                \
		                         "use cheri_bounds_set_exact instead")

#		define cequalexact(a, b) __builtin_cheri_equal_exact((a), (b))
#		pragma clang deprecated(cequalexact,                                  \
		                         "use cheri_is_equal_exact instead")

#		define ctestsubset(a, b) __builtin_cheri_subset_test(a, b)
#		pragma clang deprecated(ctestsubset, "use cheri_subset_test instead")

#		define creplenalignmask(len)                                          \
			__builtin_cheri_representable_alignment_mask(len)
#		pragma clang deprecated(                                              \
		  creplenalignmask, "use cheri_representable_alignment_mask instead")

#		define croundreplen(len)                                              \
			__builtin_cheri_round_representable_length(len)
#		pragma clang deprecated(                                              \
		  croundreplen, "use cheri_round_representable_length instead")

#		define cheri_address_get(x) __builtin_cheri_address_get(x)
#		define cheri_address_set(x, y) __builtin_cheri_address_set((x), (y))
#		define cheri_address_increment(x, y)                                  \
			__builtin_cheri_offset_increment((x), (y))
#		define cheri_base_get(x) __builtin_cheri_base_get(x)
#		define cheri_top_get(x) __builtin_cheri_top_get(x)
#		define cheri_length_get(x) __builtin_cheri_length_get(x)
#		define cheri_tag_clear(x) __builtin_cheri_tag_clear(x)
#		define cheri_tag_get(x) __builtin_cheri_tag_get(x)
#		define cheri_is_valid(x) __builtin_cheri_tag_get(x)
#		define cheri_is_invalid(x) (!__builtin_cheri_tag_get(x))
#		define cheri_is_equal_exact(x, y) __builtin_cheri_equal_exact((x), (y))
#		define cheri_is_subset(x, y) __builtin_cheri_subset_test((x), (y))
#		define cheri_permissions_get(x) __builtin_cheri_perms_get(x)
#		define cheri_permissions_and(x, y) __builtin_cheri_perms_and((x), (y))
#		define cheri_type_get(x) __builtin_cheri_type_get(x)
#		define cheri_seal(a, b) __builtin_cheri_seal((a), (b))
#		define cheri_unseal(a, b) __builtin_cheri_unseal((a), (b))
#		define cheri_bounds_set(a, b) __builtin_cheri_bounds_set((a), (b))
#		define cheri_bounds_set_exact(a, b)                                   \
			__builtin_cheri_bounds_set_exact((a), (b))
#		define cheri_subset_test(a, b) __builtin_cheri_subset_test(a, b)
#		define cheri_representable_alignment_mask(len)                        \
			__builtin_cheri_representable_alignment_mask(len)
#		define cheri_round_representable_length(len)                          \
			__builtin_cheri_round_representable_length(len)

#	endif // __ASSEMBLER__

#endif

#endif // _CHERI_BUILTINS_

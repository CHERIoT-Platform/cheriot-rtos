// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <stddef.h>
#include <stdlib.h>
#include <timeout.h>

struct SKeyStruct;
typedef struct SKeyStruct *SKey;

__BEGIN_DECLS

/**
 * Create a new sealing key.
 *
 * This function is guaranteed to complete unless the allocator has exhausted
 * the total number of sealing keys possible (2^32 - 2^24). After this point,
 * it will never succeed. A compartment that is granted access to this entry
 * point is trusted not to exhaust this resource. If you wish to allow a
 * compartment to seal objects, but do not wish to allow it to allocate new
 * sealing keys, then you should insert a proxy compartment that guarantees
 * that it will call this API once and return a single key to the caller.
 *
 * The return value from this is a capability with the permit-seal and
 * permit-unseal permissions.  Callers may remove one or both of these
 * permissions and delegate the resulting capability to allow other
 * compartments to either seal or unseal the capabilities with this key.
 *
 * If the sealing keys have been exhausted then this will return
 * null.  This API is guaranteed never to block.
 */
SKey __cheri_compartment("allocator") token_key_new(void);

/**
 * Allocate a new object with size `sz`.
 *
 * An unsealed pointer to the newly allocated object is returned in
 * `*unsealed`, the sealed pointer is returned as the return value.
 * An invalid `unsealed` pointer does not constitute an error; the caller will
 * still be given the sealed return value, assuming allocation was otherwise
 * successful.
 *
 * The `key` parameter must have both the permit-seal and permit-unseal
 * permissions.
 *
 * On error, this returns null.
 */
CHERI_SEALED(void *)
__cheri_compartment("allocator")
  token_sealed_unsealed_alloc(Timeout            *timeout,
                              AllocatorCapability heapCapability,
                              SKey                key,
                              size_t              sz,
                              void              **unsealed);

/**
 * Same as token_sealed_unsealed_alloc() without getting the unsealed
 * capability.
 *
 * The key must have the permit-seal permission.
 */
CHERI_SEALED(void *)
__cheri_compartment("allocator")
  token_sealed_alloc(Timeout            *timeout,
                     AllocatorCapability heapCapability,
                     SKey,
                     size_t);

/**
 * Unseal the object given the key.
 *
 * The key may be either a static or dynamic key (i.e. one created with the
 * `STATIC_SEALING_TYPE` macro or with `token_key_new`) and the object may be
 * either allocated dynamically (via the token APIs) or statically (via the
 * `DEFINE_STATIC_SEALED_VALUE` macro).
 *
 * Returns the unsealed object if the key and object are valid and of the
 * correct type, null otherwise.
 *
 * This function is equivalent to calling both `token_obj_unseal_static` and
 * `token_obj_unseal_dynamic` and returning the result of the first one that
 * succeeds, or null if both fail.
 */
[[cheriot::interrupt_state(disabled)]] void *
  __cheri_libcall token_obj_unseal(SKey, CHERI_SEALED(void *));

/**
 * Unseal the object given the key.
 *
 * The key must be a static sealing key (i.e. one created with the
 * `STATIC_SEALING_TYPE` macro) and the object must be a statically sealed
 * object (i.e. one created with the `DEFINE_STATIC_SEALED_VALUE` macro).
 *
 * Returns the unsealed object if the key and object are valid and of the
 * correct type, null otherwise.
 */
[[cheriot::interrupt_state(disabled)]] void *
  __cheri_libcall token_obj_unseal_static(SKey, CHERI_SEALED(void *));

/**
 * Unseal the object given the key.
 *
 * The key may be either a static or dynamic key (i.e. one created with the
 * `STATIC_SEALING_TYPE` macro or with `token_key_new`) and the object must be
 * allocated dynamically with `token_sealed_alloc` or
 * `token_sealed_unsealed_alloc`.
 *
 * Returns the unsealed object if the key and object are valid and of the
 * correct type, null otherwise.
 */
[[cheriot::interrupt_state(disabled)]] void *
  __cheri_libcall token_obj_unseal_dynamic(SKey, CHERI_SEALED(void *));

/**
 * Destroy the object given its key, freeing memory.
 *
 * The key must have the permit-unseal permission.
 *
 * Returns 0 on success. `-EINVAL` if `key` or `obj` are not valid, or they
 * don't match, or if `obj` has already been destroyed.
 */
int __cheri_compartment("allocator")
  token_obj_destroy(AllocatorCapability heapCapability,
                    SKey,
                    CHERI_SEALED(void *));

/**
 * Check whether the pair of a sealing key and a heap capability can unseal a
 * sealed object.
 *
 * Returns 0 on success, `-EINVAL` if the key or object is not valid, or one of
 * the errors from `heap_can_free` if the free would fail for other reasons.
 */
int __cheri_compartment("allocator")
  token_obj_can_destroy(AllocatorCapability heapCapability,
                        SKey                key,
                        CHERI_SEALED(void *) object);

__END_DECLS

#ifdef __cplusplus
#	include <utility>

/**
 * Helper template for representing a sealed capability created by the
 * allocator's token API.
 */
template<typename T>
class Sealed
{
	/// The raw sealed pointer
	CHERI_SEALED(T *) sealedPointer;

	public:
#	if __has_extension(cheri_sealed_pointers) &&                              \
	  !defined(CHERIOT_NO_SEALED_POINTERS)
	/// Constructor from a raw sealed pointer.
	Sealed(CHERI_SEALED(T *) sealedPointer) : sealedPointer(sealedPointer) {}
#	else
	Sealed(void *sealedPointer)
	  : sealedPointer(reinterpret_cast<SObj>(sealedPointer))
	{
	}
#	endif

	/**
	 * Explicit constructor from a sealed T*.  This is explicit because this is
	 * used only in APIs that want to expose their internal sealed type as a
	 * public type.
	 */
	template<typename U>
	explicit Sealed(U *sealedPointer)
	  : sealedPointer(reinterpret_cast<decltype(sealedPointer)>(sealedPointer))
	{
	}
	/// Implicitly convert back to the wrapped value.
	operator decltype(sealedPointer)()
	{
		return sealedPointer;
	}

	/**
	 * Explicitly convert to the real type.  This is explicit because the
	 * resulting value cannot be used as a `T*` in the general case, it can be
	 * used only as an opaque `T*` that can be unsealed to give a usable `T*`.
	 */
	CHERI_SEALED(T *) get()
	{
		return sealedPointer;
	}
	/**
	 * Return the tag of the underlying pointer
	 */
	bool is_valid()
	{
		return __builtin_cheri_tag_get(get());
	}
};

/**
 * Type-safe helper to allocate a sealed `T*`.  Returns the sealed and unsealed
 * pointers.
 *
 * Callers should check the sealed capability's tag to determine success.
 */
template<typename T>
__always_inline std::pair<T *, Sealed<T>>
token_allocate(Timeout *timeout, AllocatorCapability heapCapability, SKey key)
{
	/*
	 * Explicitly initialize unsealed, since callers like to check it, and not
	 * the sealed result, for validity.
	 */
	void *unsealed = nullptr;
	CHERI_SEALED(void *)
	sealed = token_sealed_unsealed_alloc(
	  timeout, heapCapability, key, sizeof(T), &unsealed);
	return {static_cast<T *>(unsealed),
#	if __has_extension(cheri_sealed_pointers) &&                              \
	  !defined(CHERIOT_NO_SEALED_POINTERS)
	        static_cast<CHERI_SEALED(T *)>(sealed)
#	else
	        Sealed<T>{sealed}
#	endif
	};
}

template<typename T>
__always_inline T *token_unseal(SKey key, Sealed<T> sealed)
{
	return static_cast<T *>(token_obj_unseal(key, sealed));
}

#endif // __cplusplus

#if __has_extension(cheri_sealed_pointers)
#	ifdef __cplusplus
template<typename T>
__always_inline T *token_unseal(SKey key, T *__sealed_capability sealed)
{
	return static_cast<T *>(token_obj_unseal(key, sealed));
}
#	else
#		define token_unseal(key, sealed) /*NOLINT*/                           \
			((__typeof__(*(sealed)) *)token_obj_unseal(key, sealed))
#	endif
#endif

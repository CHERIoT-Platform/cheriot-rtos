// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <stddef.h>

struct SKeyStruct;
struct SObjStruct;
typedef struct SKeyStruct *SKey;
typedef struct SObjStruct *SObj;

#define INVALID_SKEY ((SKey)0)
#define INVALID_SOBJ ((SObj)0)

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
 * `INVALID_SKEY`.  This API is guaranteed never to block.
 */
SKey __cheri_compartment("alloc") token_key_new(void);

/**
 * Allocate a new object with size `sz`.
 *
 * An unsealed pointer to the newly allocated object is returned in
 * `*unsealed`, the sealed pointer is returned as the return value.
 *
 * The `key` parameter must have both the permit-seal and permit-unseal
 * permissions.
 *
 * On error, this returns `INVALID_SOBJ`.
 */
SObj __cheri_compartment("alloc")
  token_sealed_unsealed_alloc(SKey key, size_t sz, void **unsealed);

/**
 * Same as token_sealed_unsealed_alloc() without getting the unsealed
 * capability.
 *
 * The key must have the permit-seal permission.
 */
SObj __cheri_compartment("alloc") token_sealed_alloc(SKey, size_t);

/**
 * Unseal the obj given the key.
 *
 * The key must have the permit-unseal permission.
 *
 * @return unsealed obj if key and obj are valid and they match. nullptr
 * otherwise
 */
void *__cheri_compartment("alloc") token_obj_unseal(SKey, SObj);

/**
 * Destroy the obj given its key, freeing memory.
 *
 * The key must have the permit-unseal permission.
 *
 * @return 0 if no errors. -EINVAL if key or obj not valid, or they don't
 * match, or double destroy.
 */
int __cheri_compartment("alloc") token_obj_destroy(SKey, SObj);

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
	SObj sealedPointer;

	public:
	/// Constructor from a raw sealed pointer.
	Sealed(SObj sealedPointer) : sealedPointer(sealedPointer) {}
	/**
	 * Explicit constructor from a sealed T*.  This is explicit because this is
	 * used only in APIs that want to expose their internal sealed type as a
	 * public type.
	 */
	explicit Sealed(T *sealedPointer)
	  : sealedPointer(reinterpret_cast<SObj>(sealedPointer))
	{
	}
	/// Implicitly convert back to the wrapped value.
	operator SObj()
	{
		return sealedPointer;
	}
	/**
	 * Explicitly convert to the real type.  This is explicit because the
	 * resulting value cannot be used as a `T*` in the general case, it can be
	 * used only as an opaque `T*` that can be unsealed to give a usable `T*`.
	 */
	T *get()
	{
		return reinterpret_cast<T *>(sealedPointer);
	}
};

/**
 * Type-safe helper to allocate a sealed `T*`.  Returns the sealed and unsealed
 * pointers.
 */
template<typename T>
__always_inline std::pair<T *, Sealed<T>> token_allocate(SKey key)
{
	void *unsealed;
	SObj  sealed = token_sealed_unsealed_alloc(key, sizeof(T), &unsealed);
	return {static_cast<T *>(unsealed), Sealed<T>{sealed}};
}

template<typename T>
__always_inline T *token_unseal(SKey key, Sealed<T> sealed)
{
	return static_cast<T *>(token_obj_unseal(key, sealed));
}
#endif // __cplusplus

#include <cheri.hh>
#include <debug.hh>
#include <token.h>

#include "../allocator/token.h"

using Debug = ConditionalDebug<DEBUG_TOKEN_SERVER, "Token Server">;

using namespace CHERI;

/**
 * Helper that allocates a sealed object and returns the sealed and
 * unsealed capabilities to the object.  Requires that the sealing key have
 * all of the permissions in `permissions`.
 */
static std::pair<SObj, void *>
  __noinline ts_allocate_sealed_unsealed(Token_Allocator heapAlloc,
                                         Timeout        *timeout,
                                         SObj            heapCapability,
                                         SealingKey      key,
                                         size_t          sz,
                                         PermissionSet   permissions)
{
	if (!permissions.can_derive_from(key.permissions()))
	{
		Debug::log(
		  "Operation requires {}, cannot derive from {}", permissions, key);
		return {nullptr, nullptr};
	}

	if (sz > 0xfe8 - ObjHdrSize)
	{
		Debug::log("Cannot allocate sealed object of {} bytes, too large", sz);
		// TODO: Properly handle imprecision.
		return {nullptr, nullptr};
	}

	SealedAllocation obj{
	  static_cast<SObj>(heapAlloc(timeout, heapCapability, sz + ObjHdrSize))};
	if (obj == nullptr)
	{
		Debug::log("Underlying allocation failed for sealed object");
		return {nullptr, nullptr};
	}

	obj->type   = key.address();
	auto sealed = obj;
	sealed.seal(SEALING_CAP());
	obj.address() += ObjHdrSize; // Exclude the header.
	obj.bounds() = obj.length() - ObjHdrSize;
	Debug::log("Allocated sealed {}, unsealed {}", sealed, obj);
	return {sealed, obj};
}

SObj token_sealed_unsealed_alloc(Token_Allocator heapAlloc,
                                 Timeout        *timeout,
                                 SObj            heapCapability,
                                 SKey            key,
                                 size_t          sz,
                                 void          **unsealed)
{
	auto [sealed, obj] =
	  ts_allocate_sealed_unsealed(heapAlloc,
	                              timeout,
	                              heapCapability,
	                              key,
	                              sz,
	                              {Permission::Seal, Permission::Unseal});
	*unsealed = obj;
	return sealed;
}

SObj token_sealed_alloc(Token_Allocator heapAlloc,
                        Timeout        *timeout,
                        SObj            heapCapability,
                        SKey            rawKey,
                        size_t          sz)
{
	return ts_allocate_sealed_unsealed(
	         heapAlloc, timeout, heapCapability, rawKey, sz, {Permission::Seal})
	  .first;
}

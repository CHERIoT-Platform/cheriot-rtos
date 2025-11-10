#include <cheri.hh>

using namespace CHERI;

/**
 * C API for `check_pointer`.
 *
 * See `cheri.hh` for more information.
 */
bool check_pointer(const volatile void *ptr,
                   size_t               space,
                   uint32_t             rawPermissions,
                   bool                 checkStackNeeded)
{
	auto       permissions = PermissionSet::from_raw(rawPermissions);
	Capability cap{ptr};
	bool       isValid = cap.is_valid() && !cap.is_sealed();
	// Skip the stack check if we're requiring a global capability.  By
	// construction, such a thing cannot be derived from the stack
	// pointer.
	if (checkStackNeeded)
	{
		Capability<void> stack{__builtin_cheri_stack_get()};
		// Check that the capability does not overlap the
		// stack. The base of the capability is <= its top, so
		// the capability is in bounds as long as either:
		// - its top is below the current stack or;
		// - its base is above the top of the current stack.
		isValid &= (cap.top() <= stack.base()) || (cap.base() >= stack.top());
	}
	isValid &= cap.bounds() >= space;
	// Check that we have, at least, the required permissions
	isValid &= permissions.can_derive_from(cap.permissions());
	return isValid;
}

bool __cheri_libcall check_timeout_pointer(const struct Timeout *timeout)
{
	return !heap_address_is_valid(timeout) &&
	       check_pointer(
	         timeout,
	         sizeof(struct Timeout),
	         PermissionSet{Permission::Load, Permission::Store}.as_raw(),
	         false);
}

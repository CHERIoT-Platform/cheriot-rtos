#include "cheri.hh"

/**
 * C API for `check_pointer`.
 *
 * See `cheri.hh` for more information.
 */
bool check_pointer(const void *ptr,
                   size_t      space,
                   uint32_t    rawPermissions,
                   bool        checkStackNeeded)
{
	auto permissions = CHERI::PermissionSet::from_raw(rawPermissions);
	CHERI::Capability<const void> cap{ptr};
	bool                          isValid = cap.is_valid() && !cap.is_sealed();
	// Skip the stack check if we're requiring a global capability.  By
	// construction, such a thing cannot be derived from the stack
	// pointer.
	if (checkStackNeeded)
	{
		void *csp;
		__asm__ volatile("cmove %0, csp" : "=C"(csp));
		CHERI::Capability<void> stack{csp};
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

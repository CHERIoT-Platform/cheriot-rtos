#include <cdefs.h>
#include <cheri.hh>
#include <compartment.h>

using namespace CHERI;

__cheri_compartment("stack_exhaustion_trusted") void exhaust_trusted_stack(
  __cheri_callback void (*fn)(),
  bool *outLeakedSwitcherCapability);
__cheri_compartment("stack_exhaustion_thread") void exhaust_thread_stack(
  bool *outTestFailed);
__cheri_compartment("stack_exhaustion_thread") void test_stack_permissions(
  bool      *outTestFailed,
  Permission permissionToRemove);
__cheri_compartment("stack_exhaustion_thread") void test_stack_invalid(
  bool *outTestFailed);

bool is_switcher_capability(void *reg)
{
	static constexpr PermissionSet InvalidPermissions{Permission::StoreLocal,
	                                                  Permission::Global};

	if (InvalidPermissions.can_derive_from(Capability{reg}.permissions()))
	{
		return true;
	}

	return false;
}

bool holds_switcher_capability(ErrorState *frame)
{
	for (auto reg : frame->registers)
	{
		if (is_switcher_capability(reg))
		{
			return true;
		}
	}

	return false;
}

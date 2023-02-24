#include <cdefs.h>
#include <cheri.hh>
#include <compartment.h>

using namespace CHERI;

__cheri_compartment("stack_exhaustion_trusted") void exhaust_trusted_stack(
  __cheri_callback void (*fn)(),
  bool *outLeakedSwitcherCapability);
__cheri_compartment("stack_integrity_thread") void exhaust_thread_stack(
  bool *outTestFailed);
__cheri_compartment("stack_integrity_"
                    "thread") void modify_csp_permissions_on_fault(
  bool         *outTestFailed,
  PermissionSet newPermissions);
__cheri_compartment(
  "stack_integrity_"
  "thread") void modify_stack_permissions_on_call(bool         *outTestFailed,
                                                PermissionSet newPermissions,
                                                __cheri_callback void (*fn)());
__cheri_compartment("stack_integrity_thread") void test_stack_invalid_on_fault(
  bool *outTestFailed);
__cheri_compartment("stack_integrity_thread") void test_stack_invalid_on_call(
  bool *outTestFailed,
  __cheri_callback void (*fn)());

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

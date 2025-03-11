#include <cdefs.h>
#include <cheri.hh>
#include <compartment.h>

using namespace CHERI;

__cheri_compartment("stack_integrity_thread") int exhaust_trusted_stack(
  __cheri_callback int (*fn)(),
  bool *outLeakedSwitcherCapability);
__cheri_compartment("stack_integrity_thread") int exhaust_thread_stack();
__cheri_compartment("stack_integrity_thread") int exhaust_thread_stack_spill(
  __cheri_callback int (*fn)());
__cheri_compartment("stack_integrity_thread") int set_csp_permissions_on_fault(
  PermissionSet newPermissions);
__cheri_compartment("stack_integrity_thread") int set_csp_permissions_on_call(
  PermissionSet newPermissions,
  __cheri_callback int (*fn)());
__cheri_compartment("stack_integrity_thread") int test_stack_invalid_on_fault();
__cheri_compartment("stack_integrity_thread") int test_stack_invalid_on_call(
  __cheri_callback int (*fn)());

/**
 * Sets what we expect to happen for this test.  Is a fault expected to invoke
 * the handler?  The fault handler will set or clear `*outTestFailed` when a
 * fault is received, depending on whether it was expected.
 */
__cheri_compartment("stack_integrity_thread") int set_expected_behaviour(
  bool *outTestFailed,
  bool  handlerExpected);

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

__cheri_compartment("stack_integrity_thread")
  __cheriot_minimum_stack(128) int test_stack_requirement();

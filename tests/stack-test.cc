// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Stack"
#include "stack_tests.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>

using namespace CHERI;

bool threadStackTestFailed = false;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	TEST(!holds_switcher_capability(frame),
	     "Leaked switcher capabilities to stack_test compartment");
	TEST(!threadStackTestFailed, "Thread stack test failed");

	// If we're here because of a force unwind from the callee, just continue.
	if ((mcause == 0x1c) && (mtval == 0))
	{
		return ErrorRecoveryBehaviour::InstallContext;
	}

	return ErrorRecoveryBehaviour::ForceUnwind;
}

__cheri_callback void test_trusted_stack_exhaustion()
{
	exhaust_trusted_stack(&test_trusted_stack_exhaustion,
	                      &threadStackTestFailed);
}

__cheri_callback void cross_compartment_call()
{
	TEST(false,
	     "Cross compartment call with invalid CSP shouldn't be reachable");
}

namespace
{
	PermissionSet get_stack_permissions()
	{
		Capability<void> csp = ({
			register void *cspRegister asm("csp");
			asm("" : "=C"(cspRegister));
			cspRegister;
		});

		return csp.permissions();
	}

	/**
	 * Is the stack sufficiently valid that we expect the switcher to be able
	 * to spill the register frame there?  If so, it should invoke the error
	 * handler.
	 */
	bool stack_is_mostly_valid(const PermissionSet StackPermissions)
	{
		static constexpr PermissionSet StackRequiredPermissions{
		  Permission::Load,
		  Permission::Store,
		  Permission::LoadStoreCapability,
		  Permission::StoreLocal};
		return StackRequiredPermissions.can_derive_from(StackPermissions);
	}

	void expect_handler(bool handlerExpected)
	{
		set_expected_behaviour(&threadStackTestFailed, handlerExpected);
	}
} // namespace

// Defeat the compiler optimisation that may turn our first call to this into a
// call. If the compiler does this then we will fail on an even number of
// cross-compartment calls not an odd number.
__cheri_callback void (*volatile crossCompartmentCall)();

/*
 * The stack tests should cover the edge-cases scenarios for both
 * the trusted and compartment stacks. We make sure the
 * switcher handle them correctly. We check:
 * 	- trusted stack exhaustion
 *  - compartment stack exhaustion
 *	- compartment stack with incorrect permissions
 *  - invalid compartment stack
 */
void test_stack()
{
	__cheri_callback void (*callback)() = cross_compartment_call;

	crossCompartmentCall = test_trusted_stack_exhaustion;
	debug_log("exhaust trusted stack, do self recursion with a cheri_callback");
	expect_handler(true);
	(*crossCompartmentCall)();

	debug_log("exhausting the compartment stack");
	expect_handler(false);
	exhaust_thread_stack();

	debug_log("modifying stack permissions on fault");
	PermissionSet compartmentStackPermissions = get_stack_permissions();
	for (auto permissionToRemove : compartmentStackPermissions)
	{
		auto permissions =
		  compartmentStackPermissions.without(permissionToRemove);
		debug_log("Permissions: {}", permissions);
		expect_handler(stack_is_mostly_valid(permissions));
		set_csp_permissions_on_fault(permissions);
	}

	debug_log("modifying stack permissions on cross compartment call");
	for (auto permissionToRemove : compartmentStackPermissions)
	{
		auto permissions =
		  compartmentStackPermissions.without(permissionToRemove);
		debug_log("Permissions: {}", permissions);
		set_csp_permissions_on_call(permissions, callback);
	}

	debug_log("invalid stack on fault");
	expect_handler(false);
	test_stack_invalid_on_fault();

	debug_log("invalid stack on cross compartment call");
	expect_handler(false);
	test_stack_invalid_on_call(callback);
}

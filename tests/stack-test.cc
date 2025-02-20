// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Stack"
#include "stack_tests.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>
#include <priv/riscv.h>

using namespace CHERI;

bool threadStackTestFailed = false;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	TEST(!holds_switcher_capability(frame),
	     "Leaked switcher capabilities to stack_test compartment");
	TEST(!threadStackTestFailed, "Thread stack test failed");

	// If we're here because of a force unwind from the callee, just continue.
	if ((mcause == priv::MCAUSE_CHERI) && (mtval == 0))
	{
		return ErrorRecoveryBehaviour::InstallContext;
	}

	return ErrorRecoveryBehaviour::ForceUnwind;
}

__cheri_callback int test_trusted_stack_exhaustion()
{
	return exhaust_trusted_stack(&test_trusted_stack_exhaustion,
	                             &threadStackTestFailed);
}

__cheri_callback int cross_compartment_call()
{
	TEST(false,
	     "Cross compartment call with invalid CSP shouldn't be reachable");
	return -EINVAL;
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
		// Note: This does not include store-local, but the register frame will
		// be corrupted if store local is missing because all pointers to the
		// stack, including the spilled csp, will have their tags cleared.
		static constexpr PermissionSet StackRequiredPermissions{
		  Permission::Load, Permission::Store, Permission::LoadStoreCapability};
		return StackRequiredPermissions.can_derive_from(StackPermissions);
	}

	void expect_handler(bool handlerExpected)
	{
		debug_log("Expected to invoke the handler? {}", handlerExpected);
		TEST_EQUAL(
		  set_expected_behaviour(&threadStackTestFailed, handlerExpected),
		  0,
		  "Failed to set expectations");
	}

	__attribute__((used)) extern "C" int test_small_stack()
	{
		return test_stack_requirement();
	}
} // namespace

__attribute__((used)) extern "C" int test_with_small_stack(size_t stackSize);

asm(".section .text\n"
    ".global test_with_small_stack\n"
    "test_with_small_stack:\n"
    // Preserve the old stack
    "  cmove ct0, csp\n"
    // Add space to the requested size for the spill slots and the size of the
    // stack that `test_small_stack` will use, plus the four capabilities that
    // the switcher will spill for us.  We use 16 bytes to store the stack
    // pointer and return address, `test_small_stack` uses the same amount: it
    // needs to store the return address, and the ABI requires that stacks are
    // 16-byte aligned and so it uses a full 16 bytes.
    "  add a0, a0, 64\n"
    // Get the base of the stack
    "  cgetbase a2, csp\n"
    // Move the stack pointer to the current base
    "  csetaddr csp, csp, a2\n"
    // Truncate the stack
    "  csetbounds csp, csp, a0\n"
    // Move to the end of the stack, minus the spill-slot size
    "  cincoffset csp, csp, a0\n"
    "  cincoffset csp, csp, -16\n"
    "  csc  ct0, 0(csp)\n"
    "  csc  cra, 8(csp)\n"
    // Call the test function
    "  cjal test_small_stack\n"
    // Restore
    "  clc  cra, 8(csp)\n"
    "  clc  csp, 0(csp)\n"
    "  cjr cra\n");

// Defeat the compiler optimisation that may turn our first call to this into a
// call. If the compiler does this then we will fail on an even number of
// cross-compartment calls not an odd number.
__cheri_callback int (*volatile crossCompartmentCall)();

/*
 * The stack tests should cover the edge-cases scenarios for both
 * the trusted and compartment stacks. We make sure the
 * switcher handle them correctly. We check:
 * 	- trusted stack exhaustion
 *  - compartment stack exhaustion
 *	- compartment stack with incorrect permissions
 *  - invalid compartment stack
 */
int test_stack()
{
	int ret = test_with_small_stack(144);
	TEST_EQUAL(ret, 0, "test_with_small_stack failed with 144-byte stack");
	ret = test_with_small_stack(160);
	TEST_EQUAL(ret, 0, "test_with_small_stack failed with 160-byte stack");
	ret = test_with_small_stack(128);
	TEST_EQUAL(ret,
	           -ENOTENOUGHSTACK,
	           "test_with_small_stack failed (succeeded?) with 128-byte stack");
	__cheri_callback int (*callback)() = cross_compartment_call;

	crossCompartmentCall = test_trusted_stack_exhaustion;
	debug_log("exhaust trusted stack, do self recursion with a cheri_callback");
	expect_handler(true);
	(*crossCompartmentCall)();

	debug_log("exhausting the compartment stack");
	expect_handler(false);
	TEST_EQUAL(
	  exhaust_thread_stack(), -ECOMPARTMENTFAIL, "exhaust_thread_stack failed");

	debug_log("exhausting the compartment stack during a switcher call");
	expect_handler(false);
	threadStackTestFailed = true;
	TEST_EQUAL(exhaust_thread_stack_spill(callback),
	           0,
	           "exhaust_thread_stack_spill failed");
	TEST_EQUAL(threadStackTestFailed, false, "switcher did not return error");

	debug_log("modifying stack permissions on fault");
	PermissionSet compartmentStackPermissions = get_stack_permissions();
	for (auto permissionToRemove : compartmentStackPermissions)
	{
		auto permissions =
		  compartmentStackPermissions.without(permissionToRemove);
		debug_log("Permissions: {}", permissions);
		expect_handler(stack_is_mostly_valid(permissions));
		TEST_EQUAL(set_csp_permissions_on_fault(permissions),
		           -ECOMPARTMENTFAIL,
		           "Unexpected success with restricted permissions");
	}

	debug_log("modifying stack permissions on cross compartment call");
	for (auto permissionToRemove : compartmentStackPermissions)
	{
		auto permissions =
		  compartmentStackPermissions.without(permissionToRemove);
		debug_log("Permissions: {}", permissions);
		TEST_EQUAL(set_csp_permissions_on_call(permissions, callback),
		           -ECOMPARTMENTFAIL,
		           "Unexpected success with restricted permissions");
	}

	debug_log("invalid stack on fault");
	expect_handler(false);
	TEST_EQUAL(test_stack_invalid_on_fault(),
	           -ECOMPARTMENTFAIL,
	           "stack_invalid_on_fault failed");

	debug_log("invalid stack on cross compartment call");
	expect_handler(false);
	TEST_EQUAL(test_stack_invalid_on_call(callback),
	           -ECOMPARTMENTFAIL,
	           "stack_invalid_on_call failed");

	return 0;
}

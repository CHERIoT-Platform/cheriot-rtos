#define TEST_NAME "Stack tests, stack metadata"
#include "stack_tests.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>

using namespace CHERI;

bool *threadStackTestFailed;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	TEST(false,
	     "Error handler in compartment that invalidate its stack should not be "
	     "called");
	return ErrorRecoveryBehaviour::ForceUnwind;
}

void set_csp_and_fault(Capability<void> csp)
{
	__asm__ volatile("cmove csp, %0\n" ::"C"(csp.get()));
	__asm__ volatile("csh            zero, 0(cnull)\n");
}

void test_stack_permissions()
{
	debug_log("modify the compartment stack permissions");

	Capability<void> csp = ({
		register void *cspRegister asm("csp");
		asm("" : "=C"(cspRegister));
		cspRegister;
	});

	// TODO: this should be static constexpr; In the meantime, the begin
	// function is not marked const
	PermissionSet PermissionsToRemove{
	  Permission::Load, Permission::Store, Permission::LoadStoreCapability};
	for (auto permission : PermissionsToRemove)
	{
		csp.permissions() &= csp.permissions().without(permission);
		TEST(csp.permissions().contains(permission) == false,
		     "Did not remove permission");
	}

	// Verify CSP is valid
	TEST(csp.is_valid() == true, "CSP isn't valid");

	set_csp_and_fault(csp);

	TEST(false, "Should be unreachable");
}

void test_stack_invalid()
{
	debug_log("modify the compartment stack tag");

	Capability<void> csp = ({
		register void *cspRegister asm("csp");
		asm("" : "=C"(cspRegister));
		cspRegister;
	});

	// Verify CSP is valid
	__asm__ volatile("ccleartag		csp, csp\n");
	TEST(csp.is_valid() == false, "CSP is valid");

	set_csp_and_fault(csp);

	TEST(false, "Should be unreachable");
}
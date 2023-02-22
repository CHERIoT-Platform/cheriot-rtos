#define TEST_NAME "Stack tests, exhaust thread stack"
#include "stack_tests.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>

using namespace CHERI;

bool *threadStackTestFailed;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("Error handler called, set threadStackTestFailed");
	*threadStackTestFailed = true;

	TEST(false,
	     "Error handler in compartment that exhausts its stack should not be "
	     "called");

	/* This is unreachable code, but we need to make the compiler happy
	 * because the function has a non-void return value
	 */
	return ErrorRecoveryBehaviour::ForceUnwind;
}

void exhaust_thread_stack(bool *outTestFailed)
{
	debug_log("exhausting the compartment stack");

	threadStackTestFailed = outTestFailed;
	/* Move the compartment's stack near its end, in order to
	 * trigger stack exhaustion while the switcher handles
	 * faults from the compartment.
	 *
	 * The correct behavior here is NOT executing the rest of this
	 * function NOR the error handler. Because the thread's stack is
	 * exhausted, we can't do neither. Therefore, we should enter
	 * the error handler of the caller.
	 *
	 * We use the boolean thread_stack_test_failed to indicate
	 * to the parent if any unexpected error occured.
	 */
	__asm__ volatile("cgetbase	t1, csp\n"
	                 "addi		t1, t1, 16\n"
	                 "csetaddr	csp, csp, t1\n"
	                 "csh            zero, 0(cnull)\n");

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void set_csp_and_fault(Capability<void> csp)
{
	__asm__ volatile("cmove csp, %0\n" ::"C"(csp.get()));
	__asm__ volatile("csh            zero, 0(cnull)\n");
}

void test_stack_permissions(bool *outTestFailed, Permission permissionToRemove)
{
	debug_log("modify the compartment stack permissions {}",
	          permissionToRemove);

	threadStackTestFailed = outTestFailed;

	Capability<void> csp = ({
		register void *cspRegister asm("csp");
		asm("" : "=C"(cspRegister));
		cspRegister;
	});

	csp.permissions() &= csp.permissions().without(permissionToRemove);
	TEST(csp.permissions().contains(permissionToRemove) == false,
	     "Did not remove permission");

	// Verify CSP is valid
	TEST(csp.is_valid() == true, "CSP isn't valid");

	set_csp_and_fault(csp);

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void test_stack_invalid(bool *outTestFailed)
{
	debug_log("modify the compartment stack tag");

	threadStackTestFailed = outTestFailed;

	Capability<void> csp = ({
		register void *cspRegister asm("csp");
		asm("" : "=C"(cspRegister));
		cspRegister;
	});

	// Verify CSP is valid
	__asm__ volatile("ccleartag		csp, csp\n");
	TEST(csp.is_valid() == false, "CSP is valid");

	set_csp_and_fault(csp);

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}
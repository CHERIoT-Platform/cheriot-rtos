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
	*threadStackTestFailed = true;

	TEST(false,
	     "Error handler in compartment that exhausts/invalidated its stack "
	     "should not be called");

	/* This is unreachable code, but we need to make the compiler happy
	 * because the function has a non-void return value
	 */
	return ErrorRecoveryBehaviour::ForceUnwind;
}

void exhaust_thread_stack(bool *outTestFailed)
{
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
	__asm__ volatile("cgetbase  t1, csp\n"
	                 "addi      t1, t1, 16\n"
	                 "csetaddr  csp, csp, t1\n"
	                 "csh       zero, 0(cnull)\n");

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void modify_csp_permissions_on_fault(bool         *outTestFailed,
                                     PermissionSet newPermissions)
{
	threadStackTestFailed = outTestFailed;

	__asm__ volatile(
	  "candperm csp, csp, %0\n"
	  "csh      zero, 0(cnull)\n" ::"r"(newPermissions.as_raw()));

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void modify_csp_permissions_on_call(bool         *outTestFailed,
                                    PermissionSet newPermissions,
                                    __cheri_callback void (*fn)())
{
	threadStackTestFailed = outTestFailed;

	/*
	 * TODO: at the moment, we can't call a __cheri_callback in inline assembly.
	 * Therefore, we need to rely on the compiler, and modify the stack
	 * permissions in inline assembly before the call.
	 */
	__asm__ volatile("candperm csp, csp, %0" ::"r"(newPermissions.as_raw()));
	fn();

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void test_stack_invalid_on_fault(bool *outTestFailed)
{
	threadStackTestFailed = outTestFailed;

	__asm__ volatile("ccleartag     csp, csp\n"
	                 "csh           zero, 0(cnull)\n");

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void test_stack_invalid_on_call(bool *outTestFailed,
                                __cheri_callback void (*fn)())
{
	threadStackTestFailed = outTestFailed;

	/*
	 * TODO: at the moment, we can't call a __cheri_callback in inline assembly.
	 * Therefore, we need to rely on the compiler, and clear the CSP tag
	 * in inline assembly before the call.
	 */
	__asm__ volatile("ccleartag      csp, csp");
	fn();

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}
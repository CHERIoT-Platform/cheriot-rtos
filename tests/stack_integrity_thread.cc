#define TEST_NAME "Stack tests, exhaust thread stack"
#include "stack_tests.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>

using namespace CHERI;

bool *threadStackTestFailed;

/*
 * Define a macro that gets a __cheri_callback capability and calls it, while
 * support adding instruction before the call. This is used to avoid code
 * duplication, in cases we want to call a __cheri_callback in multiple
 * places while adding additional functionalities.
 *
 *  handle: a sealed capability to a __cheri_callback to call
 *  instruction: additional instruction(s) to add before the call,
 *				   with an operand.
 *  additional_input: the operand the additional instruction refers to.
 */
#define CALL_CHERI_CALLBACK(handle, instructions, additional_input)            \
	({                                                                         \
		register auto rfn asm("ct1") = handle;                                 \
		__asm__ volatile(                                                      \
		  "1:\n"                                                               \
		  "auipcc ct2, %%cheri_compartment_pccrel_hi(.compartment_switcher)\n" \
		  "clc ct2, %%cheri_compartment_pccrel_lo(1b)(ct2)\n"                  \
		  "" instructions "\n"                                                 \
		  "cjalr ct2\n"                                                        \
		  : /* no outputs; we're jumping and probably not coming back */       \
		  : "C"(rfn), "r"(additional_input));                                  \
                                                                               \
		TEST(false, "Should be unreachable");                                  \
	})

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

void set_csp_permissions_on_fault(bool         *outTestFailed,
                                  PermissionSet newPermissions)
{
	threadStackTestFailed = outTestFailed;

	__asm__ volatile(
	  "candperm csp, csp, %0\n"
	  "csh      zero, 0(cnull)\n" ::"r"(newPermissions.as_raw()));

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void set_csp_permissions_on_call(bool         *outTestFailed,
                                 PermissionSet newPermissions,
                                 __cheri_callback void (*fn)())
{
	threadStackTestFailed = outTestFailed;

	CALL_CHERI_CALLBACK(fn, "candperm csp, csp, %1\n", newPermissions.as_raw());

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

	// the `move zero, %1` is a no-op, just to have an operand
	CALL_CHERI_CALLBACK(fn, "move zero, %1\nccleartag csp, csp\n", 0);

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}
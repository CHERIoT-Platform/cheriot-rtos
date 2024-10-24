#define TEST_NAME "Stack tests, exhaust thread stack"
#include "stack_tests.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>

using namespace CHERI;

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
		  "auipcc ct2, %%cheriot_compartment_hi(.compartment_switcher)\n"      \
		  "clc ct2, %%cheriot_compartment_lo_i(1b)(ct2)\n"                     \
		  "" instructions "\n"                                                 \
		  "cjalr ct2\n"                                                        \
		  : /* no outputs; we're jumping and probably not coming back */       \
		  : "C"(rfn), "r"(additional_input));                                  \
                                                                               \
		TEST(false, "Should be unreachable");                                  \
	})

namespace
{
	/// Is the error handler expected?
	bool expectedHandler = false;

	/// Pointer to the value in the caller used to report failure.
	bool *threadStackTestFailed;

} // namespace

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	bool leakedSwitcherCapabilities = holds_switcher_capability(frame);
	*threadStackTestFailed = !expectedHandler && !leakedSwitcherCapabilities;
	// This needs to store pointers on the stack, so may fault.
	debug_log("Error handler called in callee");

	TEST(!leakedSwitcherCapabilities,
	     "Switcher leaked privileged capabilities");

	TEST(expectedHandler,
	     "Error handler in compartment that exhausts/invalidated its stack "
	     "should not be called");

	return ErrorRecoveryBehaviour::ForceUnwind;
}

/**
 * Set up the handler expectations.  Takes the caller's error flag and
 * whether the handler is expected as arguments.
 */
void set_expected_behaviour(bool *outTestFailed, bool handlerExpected)
{
	expectedHandler       = handlerExpected;
	threadStackTestFailed = outTestFailed;
	*outTestFailed        = handlerExpected;
}

void exhaust_thread_stack()
{
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

/**
 * Arrange to exhaust the stack inside the cross-compartment switcher's spill of
 * callee-saved state.  The result should simply be an error return, rather than
 * a forced-unwind.
 */
void exhaust_thread_stack_spill(__cheri_callback void (*fn)())
{
	register auto      rfn asm("ct1") = fn;
	register uintptr_t res asm("ca0") = 0;

	__asm__ volatile(
	  // Save the stack to put back later
	  "cmove     cs0, csp\n"

	  // Shrink the available stack space
	  "cgetbase  s1, csp\n"
	  "addi      s1, s1, %[stackleft]\n"
	  "csetaddr  csp, csp, s1\n"

	  // Make the call
	  "1:\n"
	  "auipcc ct2, %%cheriot_compartment_hi(.compartment_switcher)\n"
	  "clc ct2, %%cheriot_compartment_lo_i(1b)(ct2)\n"
	  "cjalr ct2\n"

	  "cmove     csp, cs0\n"
	  : /* outs */ "+C"(res)
	  : /* ins */[stackleft] "i"(sizeof(void *))
	  : /* clobbers */ "ct2", "cs0", "cs1");

	*threadStackTestFailed = false;
	TEST(res == -ENOTENOUGHSTACK, "Bad return {}", res);
}

void set_csp_permissions_on_fault(PermissionSet newPermissions)
{
	__asm__ volatile(
	  "candperm csp, csp, %0\n"
	  "csh      zero, 0(cnull)\n" ::"r"(newPermissions.as_raw()));

	TEST(false, "Should be unreachable");
}

void set_csp_permissions_on_call(PermissionSet newPermissions,
                                 __cheri_callback void (*fn)())
{
	CALL_CHERI_CALLBACK(fn, "candperm csp, csp, %1\n", newPermissions.as_raw());

	TEST(false, "Should be unreachable");
}

void test_stack_invalid_on_fault()
{
	__asm__ volatile("ccleartag     csp, csp\n"
	                 "csh           zero, 0(cnull)\n");

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void test_stack_invalid_on_call(__cheri_callback void (*fn)())
{
	// the `move zero, %1` is a no-op, just to have an operand
	CALL_CHERI_CALLBACK(fn, "move zero, %1\nccleartag csp, csp\n", 0);

	*threadStackTestFailed = true;
	TEST(false, "Should be unreachable");
}

void self_recursion(__cheri_callback void (*fn)())
{
	(*fn)();
}

void exhaust_trusted_stack(__cheri_callback void (*fn)(),
                           bool *outLeakedSwitcherCapability)
{
	self_recursion(fn);
}

int test_stack_requirement()
{
	return 0;
}

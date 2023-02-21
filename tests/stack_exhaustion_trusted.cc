#define TEST_NAME "Stack tests, exhaust trusted stack"
#include "stack_tests.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>

using namespace CHERI;

bool *leakedSwitcherCapability;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	debug_log("Detected error in instruction {}", frame->pcc);
	debug_log("Error cause: {}", mcause);

	if (holds_switcher_capability(frame))
	{
		*leakedSwitcherCapability = true;
		TEST(false, "Leaked switcher capability");
	}

	return ErrorRecoveryBehaviour::ForceUnwind;
}

void self_recursion(__cheri_callback void (*fn)())
{
	(*fn)();
}

void exhaust_trusted_stack(__cheri_callback void (*fn)(),
                           bool *outLeakedSwitcherCapability)
{
	debug_log("exhaust trusted stack, do self recursion with a cheri_callback");
	leakedSwitcherCapability = outLeakedSwitcherCapability;
	self_recursion(fn);
}

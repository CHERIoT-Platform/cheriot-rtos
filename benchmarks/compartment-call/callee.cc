#include "callee.h"
#include "../timing.h"

int noop_return_metric()
{
	return METRIC();
}

int stack_using_return_metric(size_t stackUse)
{
	use_stack(stackUse);
	return METRIC();
}

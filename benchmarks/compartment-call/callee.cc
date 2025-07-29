#include "callee.h"
#include "../timing.h"

#include <errno.h>
#include <stdlib.h>

int callee_noop_return_metric()
{
	return METRIC();
}

int callee_stack_using_return_metric(size_t stackUse)
{
	use_stack(stackUse);
	return METRIC();
}

int callee_dereference(int *p)
{
	return *p;
}

#include "callee.h"
#include "../timing.h"

#include <errno.h>
#include <stdlib.h>

int noop_return_metric()
{
	return METRIC();
}

int stack_using_return_metric(size_t stackUse)
{
	use_stack(stackUse);
	return METRIC();
}

int check_pointer_return_metric(void *p)
{
	using namespace CHERI;

	if (!check_pointer<PermissionSet{Permission::Load}>(p))
	{
		return -EINVAL;
	}

	return METRIC();
}

int ephemeral_claim_return_metric(void *p)
{
	Timeout t{UnlimitedTimeout};
	heap_claim_ephemeral(&t, p);

	return METRIC();
}

int claim_release_return_metric(void *p)
{
	heap_claim(MALLOC_CAPABILITY, p);
	heap_free(MALLOC_CAPABILITY, p);

	return METRIC();
}

int callee_dereference(int *p)
{
	return *p;
}

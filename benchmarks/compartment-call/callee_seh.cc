#include "callee.h"

#include <errno.h>
#include <unwind.h>

int callee_seh_dereference(int *p)
{
	CHERIOT_DURING
	{
		return *p;
	}
	CHERIOT_HANDLER
	{
		return -EINVAL;
	}
	CHERIOT_END_HANDLER
}

#include "callee.h"

#include <errno.h>

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	return ForceUnwind;
}

int callee_ueh_dereference(int *p)
{
	return *p;
}

#include "callee.h"
#include "../timing.h"

void  noop() {}
int  noop_return() { return rdcycle(); }
int  noop_call(int start)
{
	return rdcycle() - start;
}


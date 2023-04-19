#include "callee.h"
#include "../timing.h"

int  noop_return(size_t s) 
{ 
	use_stack(s);
	return rdcycle(); 
}

int  noop_call(int start)
{
	int end = rdcycle();
	check_stack_zeroed();
	return end - start;
}

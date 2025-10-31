#include "helper.h"
[[cheriot::interrupt_state(enabled)]] void *help()
{
	return __builtin_return_address(0);
}

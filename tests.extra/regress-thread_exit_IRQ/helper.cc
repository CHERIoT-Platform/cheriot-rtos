#include "helper.h"
[[cheri::interrupt_state(enabled)]] void* help(void)
{
	return __builtin_return_address(0);
}

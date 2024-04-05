#include "../timing.h"
#include "callee.h"
#include <compartment.h>
#include <locks.hh>

void __cheri_compartment("caller") run()
{
	printf("#board\tstack_use\tcall\treturn\n");
	for (size_t s = 1; s < 0x1000; s <<= 1)
	{
		auto [callPath, returnPath] = CHERI::with_interrupts_disabled([&]() {
			use_stack(s);
			auto callPath   = noop_call(rdcycle());
			auto returnPath = noop_return(s);
			returnPath      = rdcycle() - returnPath;
			check_stack_zeroed();
			return std::tuple{callPath, returnPath};
		});
		printf(__XSTRING(BOARD) "\t%d\t%d\t%d\n", s, callPath, returnPath);
	}
}

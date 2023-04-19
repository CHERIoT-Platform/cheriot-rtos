#include "../timing.h"
#include "callee.h"
#include <compartment.h>
#include <locks.hh>

void __cheri_compartment("caller") run()
{
	MessageBuilder<ImplicitUARTOutput> out;
	out.format("#board\tstack_use\tcall\treturn\n");
	for (size_t s = 1; s < 0x1000; s <<= 1)
	{
		auto [callPath, returnPath] = CHERI::with_interrupts_disabled([&]() {
			use_stack(s);
			auto callPath = noop_call(rdcycle());
			auto returnPath = noop_return(s);
			returnPath = rdcycle() - returnPath;
			check_stack_zeroed();
			return std::tuple{callPath, returnPath};
		});
		out.format(__XSTRING(BOARD) "\t{}\t{}\t{}\n", s, callPath, returnPath);
	}
}

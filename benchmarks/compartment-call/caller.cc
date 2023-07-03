#include "../timing.h"
#include "callee.h"
#include <compartment.h>
#include <locks.hh>


void __cheri_compartment("caller") run()
{
	MessageBuilder<ImplicitUARTOutput> out;
	static TicketLock lock;
	static bool headerWritten;
	LockGuard g{lock};
	if (!headerWritten)
	{
		out.format("#board\tstack size\tfull call\tcall\treturn\n");
		headerWritten = true;
	}
	auto [full, callPath, returnPath] = CHERI::with_interrupts_disabled([&]() {
		auto start = rdcycle();
		auto middle = noop_return_rdcycle();
		auto end = rdcycle();
		return std::tuple{end - start, middle - start, end - middle};
	});
	size_t stackSize = get_stack_size();
	out.format(__XSTRING(BOARD) "\t{}\t{}\t{}\t{}\n", stackSize, full, callPath, returnPath);
}

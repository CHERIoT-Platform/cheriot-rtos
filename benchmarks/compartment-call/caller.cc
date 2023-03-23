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
		auto full = rdcycle();
		noop();
		full = rdcycle() - full;
		auto callPath = noop_call(rdcycle());
		auto returnPath = noop_return();
		returnPath = rdcycle() - returnPath;
		return std::tuple{full, callPath, returnPath};
	});
	register void    *cspRegister asm("csp");
	asm("" : "=C"(cspRegister));
	CHERI::Capability stack{cspRegister};
	size_t            stackSize = stack.length();
	out.format(__XSTRING(BOARD) "\t{}\t{}\t{}\t{}\n", stackSize, full, callPath, returnPath);
}

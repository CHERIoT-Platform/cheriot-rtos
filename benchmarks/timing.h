#include <cheri.hh>

namespace
{
	/**
	 * Read the cycle counter.
	 */
	int rdcycle()
	{
		int cycles;
#ifdef SAIL
		// On Sail, report the number of instructions, the cycle count is
		// meaningless.
		__asm__ volatile("csrr %0, minstret" : "=r"(cycles));
#else
		__asm__ volatile("rdcycle %0" : "=r"(cycles));
#endif
		return cycles;
	}

	/**
	 * Utility function to return the size of the current stack based on the
	 * length of csp capability register. If used in a thread entry point this
	 * will return the full length of the current stack, but if used after a
	 * compartment call csp is restricted to the part of the stack that is not
	 * in use by calling compartment(s), so the length will be smaller.
	 */
	size_t get_stack_size()
	{
		register void *cspRegister asm("csp");
		asm("" : "=C"(cspRegister));
		CHERI::Capability stack{cspRegister};
		return stack.length();
	}
} // namespace

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
#elifdef IBEX
		// CHERIoT-Ibex does not yet implement rdcycle, so read the CSR
		// directly.
		__asm__ volatile("csrr %0, mcycle" : "=r"(cycles));
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

	void use_stack(size_t s)
	{
		volatile uint8_t stack_array[s];
		stack_array[0] = 1;
	}

	void check_stack_zeroed()
	{
		register void **cspRegister asm("csp");
		asm("" : "=C"(cspRegister));
		CHERI::Capability stack{cspRegister};
		CHERI::Capability<void*> stackP{stack};
		stackP.address() = stack.base();
		while(stackP.address() < stack.address())
		{
			CHERI::Capability<void> value{*stackP};
			if (value != NULL)
			{
				__builtin_trap();
			}
			stackP += 1;
		}
	}
} // namespace


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
} // namespace

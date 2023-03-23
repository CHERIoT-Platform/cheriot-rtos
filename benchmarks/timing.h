
namespace
{
	/**
	 * Read the cycle counter.
	 */
	int rdcycle()
	{
		int cycles;
#ifdef SAIL
		// The Sail model doesn't implement the unprivileged cycle CSR in
		// M-mode only configurations, so we use mcycle instead. This requires
		// that mcycle is accessible read-only without the ASR permission.
		// Probably Sail should have the cycle CSR even without U-mode but need
		// clarification on spec.
		__asm__ volatile("csrr %0, mcycle" : "=r"(cycles));
#else
		__asm__ volatile("rdcycle %0" : "=r"(cycles));
#endif
		return cycles;
	}
} // namespace

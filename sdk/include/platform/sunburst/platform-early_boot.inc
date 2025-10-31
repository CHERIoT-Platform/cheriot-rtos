	/*
	 * The shadow memory may not be zeroed; ensure it is before we start or
	 * random capability loads will fail.
	 *
	 * boot.S will have ensured that ca4 holds the RW root cap at this point,
	 * and provides the .Lfill_block symbol.
	 */
	la_abs     a0, __export_mem_shadow
	csetaddr   ca0, ca4, a0
	la_abs     a1, __export_mem_shadow_end
	cjal       .Lfill_block

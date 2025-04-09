	// The shadow memory may not be zeroed, ensure it is before we start or
	// random capability loads will fail.
	la_abs     a0, __export_mem_shadow
	cspecialr  ca4, mtdc
	csetaddr   ca0, ca4, a0
	la_abs     a1, __export_mem_shadow_end
	cjal       .Lfill_block

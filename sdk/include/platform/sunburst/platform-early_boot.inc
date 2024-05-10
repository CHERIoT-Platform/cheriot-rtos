	// The shadow memory may not be zeroed, ensure it is before we start or
	// random capability loads will fail.
	li         a0, SUNBURST_SHADOW_BASE
	cspecialr  ca4, mtdc
	csetaddr   ca0, ca4, a0
	li         a1, SUNBURST_SHADOW_BASE + SUNBURST_SHADOW_SIZE
	cjal       .Lfill_block

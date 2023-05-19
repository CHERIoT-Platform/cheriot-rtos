	// Ugly hack. For some reason if I don't give this first load, subsequent mem
	// ops will trap on Flute-TCM.
	li				a3, 0x80000000
	cspecialr		ca4, mtdc
	csetaddr		ca4, ca4, a3
	clc				c0, 0(ca4)
	// The shadow memory may not be zeroed, ensure it is before we start or
	// random capability loads will fail.
	li				a0, FLUTE_SHADOW_BASE
	csetaddr		ca0, ca4, a0
	li				a1, FLUTE_SHADOW_BASE + FLUTE_SHADOW_SIZE
	cjal			.Lfill_block

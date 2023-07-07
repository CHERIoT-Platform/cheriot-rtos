#include <compartment-macros-asm.S>
#include <cheri-builtins.h>
#include "../allocator/token.h"

/*
 * An in-assembler, libcall-able implementation of
 *
 * [[cheri::interrupt_state(disabled)]] void *__cheri_compartment("token_server")
 * token_obj_unseal(struct SKeyStruct *, struct SObjStruct *);
 *
 * The name has been manually mangled as per the C++ rules.
 */

	.section .text._Z16token_obj_unsealP10SKeyStructP10SObjStruct,"axG", \
		@progbits,_Z16token_obj_unsealP10SKeyStructP10SObjStruct,comdat

	.hidden  _Z16token_obj_unsealP10SKeyStructP10SObjStruct
	.globl   _Z16token_obj_unsealP10SKeyStructP10SObjStruct
	.p2align 1
	.type    _Z16token_obj_unsealP10SKeyStructP10SObjStruct,@function

_Z16token_obj_unsealP10SKeyStructP10SObjStruct:
  /*
   * Register allocation:
   *
   *  - ca0 holds a sealing key, either the user's or the real deal, and is
   *    replaced with the unsealed value or NULL
   *
   *  - ca1 holds the user's sealed object pointer
   *
   *  - t0/ct0 holds a copy of the user key
   *
   *  - t1/ct1 is used within each local computation and never holds secrets
   */

  /* Verify key tag */
  cgettag t1, ca0
  beqz    t1, .Lexit_failure

  /* Verify key address == base and len > 0 */
  cgetbase t1, ca0 
  bne      a0, t1, .Lexit_failure // as-integer access to ca0 gives address
  cgetlen  t1, ca0
  beqz     t1, .Lexit_failure

  /* Verify key has unseal permission */
  cgetperm t1, ca0
  andi     t1, t1, CHERI_PERM_UNSEAL
  beqz     t1, .Lexit_failure

  /* Copy key capability to scratch register */
  cmove ct0, ca0

  /*
   * Load unsealing root capability, to be clobbered by return value
   * This faults only if something has gone very, very wrong, and exposes no
   * secrets if so.
   */
.Lload_sealing_key:
  auipcc ca0, %cheri_compartment_pccrel_hi(__sealingkey)
  clc    ca0, %cheri_compartment_pccrel_lo(.Lload_sealing_key)(ca0)

  /* Unseal, clobbering authority */
  cunseal ca0, ca1, ca0

  /* Verify tag of unsealed form */
  cgettag t1, ca0
  beqz    t1, .Lexit_failure

  /*
   * Load software type tag.  This will not trap, thanks to above tag check and
   * because IRQs are deferred (see our export entry above)
   */
  clw t1, TokenSObj_offset_type(ca0)

  /*
   * Verify that the loaded value matches the address of the key (via as-integer
   * access to capability register ct0).
   */
  bne t0, t1, .Lexit_failure

  /* Subset bounds to ->data */
  cgetlen         t1, ca0
  cincoffset      ca0, ca0, TokenSObj_offset_data
  addi            t1, t1, -TokenSObj_offset_data
  csetboundsexact ca0, ca0, t1

  /* And that's an unwrap. */
  cret

.Lexit_failure:
  /* Failure; clobber potential sensitive state in ca0 and return null */
  cmove ca0, cnull
  cret

/* TODO: Eventually this goes away, when the assembler can generate it for us */
CHERIOT_EXPORT_FUNCTION \
  _Z16token_obj_unsealP10SKeyStructP10SObjStruct, \
  0 /* No stack usage */, \
  0b00010010 /* IRQs deferred, zero two registers */


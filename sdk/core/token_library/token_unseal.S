#include <compartment-macros-asm.S>
#include <cheri-builtins.h>
#include "../allocator/token.h"

.include "assembly-helpers.s"

	.hidden __sealingkey_either
	.type   __sealingkey_either,@object
	.section    .sealing_key1,"aw",@progbits
	.globl  __sealingkey_either
	.p2align    3
__sealingkey_either:
	.chericap   0
	.size   __sealingkey_either, 8

	.hidden __sealingkey_static
	.type   __sealingkey_static,@object
	.section    .sealing_key2,"aw",@progbits
	.globl  __sealingkey_static
	.p2align    3
__sealingkey_static:
	.chericap   0
	.size   __sealingkey_static, 8

	.hidden __sealingkey_dynamic
	.type   __sealingkey_dynamic,@object
	.section    .sealing_key3,"aw",@progbits
	.globl  __sealingkey_dynamic
	.p2align    3
__sealingkey_dynamic:
	.chericap   0
	.size   __sealingkey_dynamic, 8

.section .text,"ax",@progbits

.p2align 1

/**
 * The core of unsealing:
 *
 *   void *token_unseal_internal(
 *     struct SKeyStruct *, struct SObjStruct *, void *);
 */
.Ltoken_unseal_internal:
  /*
   * Register allocation:
   *
   *  - ca0 holds the user's sealing key, and is replaced with the unsealed
   *    value or NULL
   *
   *  - ca1 holds the user's sealed object pointer
   *
   *  - ca2 holds the unsealing authority and is clobbered on failure
   *    explicitly and on success with a scalar (the sealed payload's length)
   *
   *  - t1 is used within each local computation and never holds secrets
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

  /* Unseal, clobbering authority */
  cunseal ca2, ca1, ca2

  /* Verify tag of unsealed form */
  cgettag t1, ca2
  beqz    t1, .Lexit_failure

  /*
   * Load software type tag.  This will not trap, thanks to above tag check and
   * because IRQs are deferred (see our export entry below)
   */
  clw t1, TokenSObj_offset_type(ca2)

  /* Verify that the loaded value matches the address of the key. */
  bne a0, t1, .Lexit_failure

  /* Subset bounds to ->data */
  // Get the top into t1
  cgettop         t1, ca2
  // Move the address to the start of the data, clobber the user's sealing key
  cincoffset      ca0, ca2, TokenSObj_offset_data
  // Subtract the address of the (to-be-returned-unsealed) data from the top to
  // give the length, clobbering our unsealing key.
  sub             a2, t1, a0
  // Set the new bounds, using an exact setting so that any errors in the
  // allocator's alignment turn into an untagged capability here.
  csetboundsexact ca0, ca0, a2

  /* And that's an unwrap. */
  cret

.Lexit_failure:
  /* Failure; clobber potential sensitive state in ca2 and return null */
  zeroOne a2
  zeroOne a0
  cret

/**
 * An in-assembler implementation of
 *
 * [[cheri::interrupt_state(disabled)]] void *__cheri_libcall
 * token_obj_unseal(struct SKeyStruct *, struct SObjStruct *);
 *
 * The name has been manually mangled as per the C++ rules.
 */
	.hidden _Z16token_obj_unsealP10SKeyStructP10SObjStruct
	.globl  _Z16token_obj_unsealP10SKeyStructP10SObjStruct
_Z16token_obj_unsealP10SKeyStructP10SObjStruct:
	LoadCapPCC ca2, __sealingkey_either

	/*
	 * Backwards compatibility with CUnseal that requires address match.
	 * This can (and should) be removed once everyone's caught up with
	 * https://github.com/CHERIoT-Platform/cheriot-sail/pull/87 .
	 */
	cgettype   t1, ca1
	csetaddr   ca2, ca2, t1

	j        .Ltoken_unseal_internal

/**
 * An in-assembler implementation of
 *
 * [[cheri::interrupt_state(disabled)]] void *__cheri_libcall
 * token_obj_unseal_static(struct SKeyStruct *, struct SObjStruct *);
 *
 * The name has been manually mangled as per the C++ rules.
 */
	.hidden  _Z23token_obj_unseal_staticP10SKeyStructP10SObjStruct
	.globl   _Z23token_obj_unseal_staticP10SKeyStructP10SObjStruct
_Z23token_obj_unseal_staticP10SKeyStructP10SObjStruct:
	LoadCapPCC ca2, __sealingkey_static
	j          .Ltoken_unseal_internal

/**
 * An in-assembler implementation of
 *
 * [[cheri::interrupt_state(disabled)]] void *__cheri_libcall
 * token_obj_unseal_dynamic(struct SKeyStruct *, struct SObjStruct *);
 *
 * The name has been manually mangled as per the C++ rules.
 */
	.hidden  _Z24token_obj_unseal_dynamicP10SKeyStructP10SObjStruct
	.globl   _Z24token_obj_unseal_dynamicP10SKeyStructP10SObjStruct
_Z24token_obj_unseal_dynamicP10SKeyStructP10SObjStruct:
	LoadCapPCC ca2, __sealingkey_dynamic
	j          .Ltoken_unseal_internal

/* TODO: Eventually this goes away, when the assembler can generate it for us */
CHERIOT_EXPORT_LIBCALL \
  _Z16token_obj_unsealP10SKeyStructP10SObjStruct, \
  0 /* No stack usage */, \
  0b00010010 /* IRQs deferred, zero two registers */

CHERIOT_EXPORT_LIBCALL \
  _Z23token_obj_unseal_staticP10SKeyStructP10SObjStruct, \
  0 /* No stack usage */, \
  0b00010010 /* IRQs deferred, zero two registers */

CHERIOT_EXPORT_LIBCALL \
  _Z24token_obj_unseal_dynamicP10SKeyStructP10SObjStruct, \
  0 /* No stack usage */, \
  0b00010010 /* IRQs deferred, zero two registers */

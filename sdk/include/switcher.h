#pragma once
#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>

/**
 * \file
 *
 * APIs for interacting with the switcher.
 */

/**
 * Returns true if the trusted stack contains at least `requiredFrames` frames
 * past the current one, false otherwise.
 */
__cheri_libcall _Bool trusted_stack_has_space(int requiredFrames);

/**
 * Recover the stack value that was passed into a compartment on
 * cross-compartment call.  This allows sub-compartment compartmentalization
 * (e.g. running another OS in a compartment) where the monitor in the
 * compartment uses heap-allocated stacks for the nested compartments, while
 * providing a mechanism for the monitor to recover the stack.
 */
__cheri_libcall void *switcher_recover_stack(void);

/**
 * Returns a store-only capability to two hazard pointer slots for the current
 * thread.  Objects stored here will not be deallocated until (at least) the
 * next cross-compartment call or until they are explicitly overwritten.
 */
__cheri_libcall void **switcher_thread_hazard_slots(void);

/**
 * Returns the lowest address that has been stored to on the stack in this
 * compartment invocation.
 */
__cheri_libcall ptraddr_t stack_lowest_used_address(void);

/**
 * Resets the switcher's count of invocations.
 *
 * Switcher place a limit on the number of times a compartment invocation may
 * fault (default 512).  This prevents a compartment from getting stuck
 * partially recovering from errors.  This function resets the count to zero.
 * It should be called from outer compartments' run loops and from places where
 * the caller is certain that error handling is making forward progress.
 *
 * Note: If this is called from an error handler that subsequently returns with
 * with install-context, the switcher will subtract one from this and set the
 * switcher error count to -1.  Because this is odd, the switcher will detect
 * the next invocation of the error handler as a double fault and will force
 * unwind.  Do not call this if you are currently in an error handler unless
 * you are jumping out via some mechanism that does *not* involve returning to
 * the switcher.
 *
 * Returns the previous value of the invocation count.  The low bit is set if
 * an error handler is currently running, the remaining bits are the count.
 */
__cheri_libcall uint16_t switcher_handler_invocation_count_reset(void);

/**
 * Get and/or modify the platform-specific CPU state associated with the current
 * compartment invocation (that is, the intersection of this thread and
 * compartment).  This state is inherited from the caller, may be modified
 * arbitrarily (within platform constraints), and will be restored when the
 * callee returns.  The exact meaning of these bits is platform-specific, to be
 * provided by platform-switcher_cpu_features.hh.  However, these C preprocessor
 * macro names are reserved for generic, cross-platform use:
 *
 * * `SWITCHER_CPU_FEATURE_CACHE_INSTRUCTION_ENABLED` -- a value which may be
 *   ORed into this state word to enable the CPU instruction cache for
 *   subsequent fetches, or whose bitwise complement may be ANDed in to this
 *   state to disable the instruction cache for subsequent fetches.
 *
 * * `SWITCHER_CPU_FEATURE_CACHE_DATA_ENABLED` -- as above, but for the data
 *   caches and memory fetches and stores.
 *
 * * `SWITCHER_CPU_FEATURE_DEFAULT` -- the loader uses this value to initialize
 *   all the threads' platform-specific CPU state.
 *
 * Platform-specific names should begin with `SWITCHER_CPU_FEATURE_PLATFORM_` to
 * avoid collision with future generic values.
 *
 * This function returns the platform state prior to making any requested
 * changes.  Denoting that value `vOld`, the new value, `vNew`, is computed as
 * `vNew = (vOld & bitsAnd) | bitsOr`.  This new value is then used to update
 * platform-specific features' effects.
 *
 * If bitsOr is 0 and bitsAnd is UINT32_MAX, no changes will be made and the
 * current value will be reported.  The value may be set exactly to bitsOr by
 * passing a bitsAnd of 0, though that may have unanticipated platform-specific
 * effects.
 */
__cheri_libcall uint32_t switcher_invocation_cpu_features_set(
  uint32_t bitsAnd, uint32_t bitsOr);

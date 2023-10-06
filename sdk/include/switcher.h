#pragma once
#include <cdefs.h>

/**
 * Returns true if the trusted stack contains at least `requiredFrames` frames
 * past the current one, false otherwise.
 *
 * Note: This is faster than calling either `trusted_stack_index` or
 * `trusted_stack_size` and so should be preferred in guards.
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
 * Returns a sealed capability to the current thread.  This can be used as the
 * argument to `switcher_interrupt_thread` from another thread to interrupt
 * this thread (invoking its error handler) if and only if the other thread is
 * currently executing in this compartment.  This can be used to implement
 * watchdog events from another thread.
 */
__cheri_libcall void *switcher_current_thread(void);

/**
 * Returns a sealed capability to the current thread.  This can be used with
 * `switcher_interrupt_thread` from another thread to interrupt this thread
 * (invoking its error handler) if and only if the other thread is currently
 * executing in this compartment.  This can be used to implement watchdog
 * events from another thread.
 *
 * Returns true on success, false on failure.
 *
 * This should typically be followed by a yielding operation.  The interrupted
 * thread is not woken until the scheduler runs.
 */
__cheri_libcall _Bool switcher_interrupt_thread(void*);

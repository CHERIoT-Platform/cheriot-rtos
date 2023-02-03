// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <stdbool.h>
#include <stdint.h>
#include <timeout.h>

/*
 * Event group APIs.
 * Event groups can be used to signal events and synchronise between multiple
 * threads. A thread specifies the event bits it waits on, and blocks until
 * another thread sets those bits. For example, a consumer thread can wait on
 * a set of bits, each representing a work queue and wakes up if any of the bits
 * are set by the producer. Certain scenarios may require waking up only when
 * all bits are set, like join()ing a set of threads for synchronisation.
 */

__BEGIN_DECLS

/**
 * Create a new event group.
 *
 * @param ret storage for the returned sealed handle
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") event_create(void **ret);

/**
 * Wait on this event group for a particular set of bits.
 *
 * @param evt sealed event group handle
 * @param retBits Stores the event bits before we return. If clearOnExit is
 * set, this stores the bits before clearing.
 * @param bitsToWait the bit mask of the bits we wait on. To be compatible with
 * FreeRTOS, the top 8 bits are reserved and bitsToWait cannot be 0.
 * @param clearOnExit Clear the bits we waited on when waking up.
 * @param waitAll Only wake up when all the bits in bitsToWait are set.
 * @param timeout The timeout for this call.
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") event_bits_wait(void     *evt,
                                                 uint32_t *retBits,
                                                 uint32_t  bitsToWait,
                                                 bool      clearOnExit,
                                                 bool      waitAll,
                                                 Timeout  *timeout);

/**
 * Set the bits in an event group.
 *
 * @param evt sealed event group handle
 * @param retBits the bits in the event group on return. If a thread is
 * waiting and specified clearOnExit, this stores the bits after clearing the
 * bits the waiter waited on.
 * @param bitsToSet the bits to set. Only the bottom 24 are allowed.
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched")
  event_bits_set(void *evt, uint32_t *retBits, uint32_t bitsToSet);

/**
 * Fetch the current event bits of this event group.
 *
 * @param evt sealed event group handle
 * @param retBits storage for the event bits
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") event_bits_get(void *evt, uint32_t *retBits);

/**
 * Manually clear the bits in event group.
 *
 * @param evt sealed event group handle
 * @param retBits storage for the event bits before clearing
 * @param bitsToClear bit mask of the bits to be cleared
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched")
  event_bits_clear(void *evt, uint32_t *retBits, uint32_t bitsToClear);

/**
 * Delete this event group. All blockers will be woken up.
 *
 * @param evt sealed event group handle
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") event_delete(void *evt);

__END_DECLS

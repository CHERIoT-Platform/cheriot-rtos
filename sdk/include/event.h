#pragma once
/**
 * This file contains the interface for an event-group API, implemented in the
 * `event_group` library, which provides a mechanism for wait for one or more
 * events from a set of 24 to occur.
 *
 * This API is provided to ease porting from FreeRTOS.  The event group
 * abstraction is not a good design and is difficult to use correctly.  It is
 * not recommended for new code.  CHERIoT RTOS provides a futex as the
 * low-level primitive on which event groups are implemented.  For new code,
 * you will almost certainly be able to build a cleaner and less error-prone
 * abstraction on top of futexes directly.
 */

#include "cdefs.h"
#include "thread.h"
#include <compartment-macros.h>
#include <stdint.h>
#include <stdlib.h>
#include <timeout.h>

struct EventGroup;

/**
 * Create a new event group, allocated using `heapCapability`.  The event group
 * is returned via `outGroup`.
 *
 * This returns zero on success.  Otherwise it returns a negative error code.
 * If the timeout expires then this returns `-ETIMEDOUT`, if memory cannot be
 * allocated it returns `-ENOMEM`.
 */
int __cheri_libcall eventgroup_create(struct Timeout     *timeout,
                                      AllocatorCapability heapCapability,
                                      struct EventGroup **outGroup);

/**
 * Wait for events in an event group.  The `bitsWanted` argument must contain
 * at least one bit set in the low 24 bits (and none in the high bits).  This
 * indicates the specific events to wait for.  If `waitForAll` is true then all
 * of the bits in `bitsWanted` must be set in the event group before this
 * returns.  If `waitForAll` is false then any of the bits in `bitsWanted`
 * being set in the event group will cause this to return.
 *
 * If this returns zero then `outBits` will contain the bits that were set at
 * the time that the condition became true.  If this returns `-ETIMEDOUT` then
 * `outBits` will contain the bits that were set at the time that the timeout
 * expired.
 *
 * Note: `waitForAll` requires all bits to be set *at the same time*.  This
 * makes it trivial to introduce race conditions if used with multiple waiters
 * and `clearOnExit`, or if different threads clear different bits in the
 * waited set.
 *
 * If `clearOnExit` is true and this returns successfully then the bits in
 * `bitsWanted` will be cleared in the event group before this returns.
 */
int __cheri_libcall eventgroup_wait(Timeout           *timeout,
                                    struct EventGroup *group,
                                    uint32_t          *outBits,
                                    uint32_t           bitsWanted,
                                    _Bool              waitForAll,
                                    _Bool              clearOnExit);

/**
 * Set one or more bits in an event group.  The `bitsToSet` argument contains
 * the bits to set.  Any thread waiting with `eventgroup_wait` will be woken if
 * the bits that it is waiting for are set.
 *
 * This returns zero on success.  If the timeout expires before this returns
 * then it returns `-ETIMEDOUT`.
 *
 * Independent of success or failure, `outBits` will be used to return the set
 * of currently set bits in this event group.
 */
int __cheri_libcall eventgroup_set(Timeout           *timeout,
                                   struct EventGroup *group,
                                   uint32_t          *outBits,
                                   uint32_t           bitsToSet);

/**
 * Clear one or more bits in an event group.  The `bitsToClear` argument
 * contains the set of bits to clear.  This does not wake any threads.
 *
 * This returns zero on success.  If the timeout expires before this returns
 * then it returns `-ETIMEDOUT`.
 *
 * Independent of success or failure, `outBits` will be used to return the set
 * of currently set bits in this event group.
 */
int __cheri_libcall eventgroup_clear(Timeout           *timeout,
                                     struct EventGroup *group,
                                     uint32_t          *outBits,
                                     uint32_t           bitsToClear);

/**
 * Returns the current value of the event bits via `outBits`.  Returns 0 on
 * success (there is currently no way in which this call can fail).
 *
 * This API is inherently racy.  Any arbitrary set of bits may be set or
 * cleared in between this call reading from the event group and returning.
 */
int __cheri_libcall eventgroup_get(struct EventGroup *group, uint32_t *outBits);

/**
 * Destroy an event group.  This forces all waiters to wake and frees the
 * underlying memory.
 */
int __cheri_libcall eventgroup_destroy(AllocatorCapability heapCapability,
                                       struct EventGroup  *group);

/**
 * Destroy an event group without tacking the lock.
 *
 * This API is inherently racy. Its main purpose is to cleanup the event group
 * in an error handler context, when taking lock may be impossible.
 */
int __cheri_libcall eventgroup_destroy_force(AllocatorCapability heapCapability,
                                             struct EventGroup  *group);

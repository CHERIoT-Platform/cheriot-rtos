// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <timeout.h>

/*
 * Semaphore APIs.
 * This is a FreeRTOS-compatible semaphore implementation. A semaphore is
 * created with a max count of N and initial count of 0. Each take() operation
 * increments the count and blocks at the maximum. Each give() decrements the
 * count and blocks at 0.
 */

__BEGIN_DECLS

/**
 * Create a new semaphore.
 *
 * @param ret storage for the returned sealed semaphore handle
 * @param maxNItems the max count the semaphore can be taken
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") semaphore_create(void **ret, size_t maxNItems);

/**
 * Delete semaphore, waking up all blockers.
 *
 * @param sema sealed semaphore handle
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") semaphore_delete(void *sema);

/**
 * Take the semaphore. If its count has reached the maximum, block.
 *
 * @param sema sealed semaphore handle
 * @param timeout The timeout for this call.
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") semaphore_take(void *sema, Timeout *timeout);

/**
 * Release the semaphore. If its count has reached 0, block.
 *
 * @param sema sealed semaphore handle
 * @param timeout The timeout for this call.
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") semaphore_give(void *sema, Timeout *timeout);

__END_DECLS

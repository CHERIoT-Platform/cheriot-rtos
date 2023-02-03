// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <compartment.h>
#include <stddef.h>
#include <stdint.h>
#include <timeout.h>

/*
 * Queue APIs.
 * Queues are FIFO structures with fixed-size messages. This queue
 * implementation allows for multiple senders and receivers, and the same queue
 * handle is used for both sending and receiving. Messages are always copied to
 * and from the callers instead of taking pointers. Each queue has a maximum
 * number of messages it can store, and callers can specify a timeout that a
 * send and recv operation can be blocked for if the queue is
 * full or empty.
 */

__BEGIN_DECLS

/**
 * Create a new message queue.
 *
 * @param outQue storage for the returned sealed queue handle. The storage
 * capability must have store and store capability permissions and at least
 * enough space for sizeof(void *). It can be a local capability.
 * @param itemSize size of each message
 * @param maxNItems max number of messages
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched")
  queue_create(void **outQue, size_t itemSize, size_t maxNItems);

/**
 * Delete this queue. All blockers will be woken up.
 *
 * @param que sealed queue handle
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched") queue_delete(void *que);

/**
 * Send a message to the queue, blocking for at most waitTicks of timer
 * ticks. The message size does not need to be provided, since we get
 * the size internally.
 *
 * @param que sealed queue handle
 * @param src the message to be sent
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched")
  queue_send(void *que, const void *src, Timeout *timeout);

/**
 * Same as queue_send, just in the other direction.
 */
int __cheri_compartment("sched")
  queue_recv(void *que, void *dst, Timeout *timeout);

/**
 * Check the number of remaining items in the queue.
 *
 * @param que sealed queue handle
 * @param items buffer of the return value
 *
 * @return error code. 0 on success
 */
int __cheri_compartment("sched")
  queue_items_remaining(void *que, size_t *items);

__END_DECLS

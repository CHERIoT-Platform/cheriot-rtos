// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT
/**
 * This file contains the interface for a simple message queue.  This is split
 * into two layers.  The core functionality is implemented as a shared library.
 * This allows message queues to be used for inter-thread communication without
 * requiring cross-compartment calls except in the blocking cases (reading from
 * an empty queue, writing to a full queue).
 *
 * These library interfaces are then wrapped in a compartment, which provides
 * sealed capabilities that authorise sending or receiving messages via a
 * queue.  The compartment interface can be used between mutually distrusting
 * compartments.  Neither endpoint can corrupt the queue state, though there is
 * of course no guarantee that the sender will send valid data.
 *
 * Both sets of queues support multiple senders and multiple receivers.  This
 * does *not* guarantee priority propagation and so a high-priority thread
 * sending a message may be starved by a low-priority thread that attempts to
 * send a message over the same queue and is preempted by a medium-priority
 * thread.
 */

#pragma once

#include "cdefs.h"
#include <multiwaiter.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <timeout.h>

/**
 * A handle to a queue endpoint.
 *
 * Dropping permissions can make this a receive-only or a send-only handle.
 */
struct QueueHandle
{
	/**
	 * The size of one element in this queue.
	 */
	size_t elementSize;
	/**
	 * The size of the queue.
	 */
	size_t queueSize;
	/**
	 * The buffer used for storing queue elements.
	 */
	void *buffer;
	/**
	 * The producer counter.
	 */
	_Atomic(uint32_t) *producer;
	/**
	 * The consumer counter.
	 */
	_Atomic(uint32_t) *consumer;
};

__BEGIN_DECLS

/**
 * Allocates space for a queue using `heapCapability` and stores a handle to it
 * via `outQueue`.  The underlying allocation (which is necessary to free the
 * queue) is returned via `outAllocation`.
 *
 * The queue is has space for `elementCount` entries.  Each entry is a fixed
 * size, `elementSize` bytes.
 */
int __cheri_libcall queue_create(Timeout            *timeout,
                                 struct SObjStruct  *heapCapability,
                                 struct QueueHandle *outQueue,
                                 void              **outAllocation,
                                 size_t              elementSize,
                                 size_t              elementCount);

/**
 * Destroys a queue. This wakes up all threads waiting to produce or consume,
 * and makes them fail to acquire the lock, before deallocating the underlying
 * allocation.
 *
 * This must be called on an unrestricted queue handle (*not* one returned by
 * `queue_make_receive_handle` or `queue_make_send_handle`).
 *
 * Returns 0 on success. On failure, returns `-EPERM` if the queue handle is
 * restricted (see comment above).
 */
int __cheri_libcall queue_destroy(struct SObjStruct  *heapCapability,
                                  struct QueueHandle *handle);

/**
 * Convert a queue handle returned from `queue_create` into one that can be
 * used *only* for receiving.
 *
 * Note: This is primarily defence in depth.  A malicious holder of this queue
 * handle can still set the consumer counter to invalid values.
 */
struct QueueHandle __cheri_libcall
queue_make_receive_handle(struct QueueHandle handle);

/**
 * Convert a queue handle returned from `queue_create` into one that can be
 * used *only* for sending.
 *
 * Note: This is primarily defence in depth.  A malicious holder of this queue
 * handle can still set the producer counter to invalid values and overwrite
 * arbitrary queue locations.
 */
struct QueueHandle __cheri_libcall
queue_make_send_handle(struct QueueHandle handle);

/**
 * Send a message to the queue specified by `handle`.  This expects to be able
 * to copy the number of bytes specified by `elementSize` when the queue was
 * created from `src`.
 *
 * Returns 0 on success.  On failure, returns `-ETIMEOUT` if the timeout was
 * exhausted, `-EINVAL` on invalid arguments.
 *
 * This expected to be called with a valid queue handle.  It does not validate
 * that this is correct.  It uses `safe_memcpy` and so will check the buffer.
 */
int __cheri_libcall queue_send(Timeout            *timeout,
                               struct QueueHandle *handle,
                               const void         *src);

/**
 * Receive a message over a queue specified by `handle`.  This expects to be
 * able to copy the number of bytes specified by `elementSize`.  The message is
 * copied to `dst`, which must have sufficient permissions and space to hold
 * the message.
 *
 * Returns 0 on success, `-ETIMEOUT` if the timeout was exhausted, `-EINVAL` on
 * invalid arguments.
 */
int __cheri_libcall queue_receive(Timeout            *timeout,
                                  struct QueueHandle *handle,
                                  void               *dst);

/**
 * Returns the number of items in the queue specified by `handle` via `items`.
 *
 * Returns 0 on success.  This has no failure mechanisms, but is intended to
 * have the same interface as the version that operates on a sealed queue
 * handle.
 *
 * Note: This interface is inherently racy.  The number of items in the queue
 * may change in between the return of this function and the caller acting on
 * the result.
 */
int __cheri_libcall queue_items_remaining(struct QueueHandle *handle,
                                          size_t             *items);

/**
 * Allocate a new message queue that is managed by the message queue
 * compartment.  This is returned as two sealed pointers to send and receive
 * ends of the queue.
 */
int __cheri_compartment("message_queue")
  queue_create_sealed(Timeout            *timeout,
                      struct SObjStruct  *heapCapability,
                      struct SObjStruct **outQueueSend,
                      struct SObjStruct **outQueReceive,
                      size_t              elementSize,
                      size_t              elementCount);

/**
 * Destroy a queue using a sealed queue endpoint handle.  The queue is not
 * actually freed until *both* endpoints are destroyed, which means that you
 * can safely call this from the sending end without the receiving end losing
 * access to messages held in the queue.
 */
int __cheri_compartment("message_queue")
  queue_destroy_sealed(Timeout           *timeout,
                       struct SObjStruct *heapCapability,
                       struct SObjStruct *queueHandle);

/**
 * Send a message via a sealed queue endpoint.  This behaves in the same way as
 * `queue_send`, except that it will return `-EINVAL` if the endpoint is not a
 * valid sending endpoint and may return `-ECOMPARTMENTFAIL` if the queue is
 * destroyed during the call.
 */
int __cheri_compartment("message_queue")
  queue_send_sealed(Timeout           *timeout,
                    struct SObjStruct *handle,
                    const void        *src);

/**
 * Receive a message via a sealed queue endpoint.  This behaves in the same way
 * as `queue_receive`, except that it will return `-EINVAL` if the endpoint is
 * not a valid receiving endpoint and may return `-ECOMPARTMENTFAIL` if the
 * queue is destroyed during the call.
 */
int __cheri_compartment("message_queue")
  queue_receive_sealed(Timeout *timeout, struct SObjStruct *handle, void *dst);

/**
 * Returns, via `items`, the number of items in the queue specified by `handle`.
 * Returns 0 on success.
 *
 * This call is intended to be fast and so does minimal checking of arguments.
 * It does not mutate state or acquire any locks and so may return
 * `-ECOMPARTMENTFAIL` for any failure case.
 */
int __cheri_compartment("message_queue")
  queue_items_remaining_sealed(struct SObjStruct *handle, size_t *items);

/**
 * Initialise an event waiter source so that it will wait for the queue to be
 * ready to receive.  Note that this is inherently racy because another consumer
 * may drain the queue before this consumer wakes up.
 */
void __cheri_libcall
multiwaiter_queue_receive_init(struct EventWaiterSource *source,
                               struct QueueHandle       *handle);

/**
 * Initialise an event waiter source so that it will wait for the queue to be
 * ready to send.  Note that this is inherently racy because another producer
 * may fill the queue before this consumer wakes up.
 */
void __cheri_libcall
multiwaiter_queue_send_init(struct EventWaiterSource *source,
                            struct QueueHandle       *handle);

/**
 * Initialise an event waiter source as in `multiwaiter_queue_receive_init`,
 * using a sealed queue endpoint.  The `source` argument must be a receive
 * endpoint.
 *
 * Returns 0 on success, `-EINVAL` on invalid arguments.  May return
 * `-ECOMPARTMENTFAIL` if the queue is deallocated in the middle of this call.
 */
int __cheri_compartment("message_queue")
  multiwaiter_queue_receive_init_sealed(struct EventWaiterSource *source,
                                        struct SObjStruct        *handle);

/**
 * Initialise an event waiter source as in `multiwaiter_queue_send_init`,
 * using a sealed queue endpoint.  The `source` argument must be a send
 * endpoint.
 *
 * Returns 0 on success, `-EINVAL` on invalid arguments.  May return
 * `-ECOMPARTMENTFAIL` if the queue is deallocated in the middle of this call.
 */
int __cheri_compartment("message_queue")
  multiwaiter_queue_send_init_sealed(struct EventWaiterSource *source,
                                     struct SObjStruct        *handle);

__END_DECLS

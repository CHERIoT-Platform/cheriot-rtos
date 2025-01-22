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
 * Structure representing a queue.  This structure represents the queue
 * metadata, the buffer is stored at the end.
 *
 * A queue is a ring buffer of fixed-sized elements with a producer and consumer
 * counter.
 */
struct MessageQueue
{
	/**
	 * The size of one element in this queue.  This should not be modified after
	 * construction.
	 */
	size_t elementSize;
	/**
	 * The size of the queue.  This should not be modified after construction.
	 */
	size_t queueSize;
	/**
	 * The producer counter.
	 */
	_Atomic(uint32_t) producer;
	/**
	 * The consumer counter.
	 */
	_Atomic(uint32_t) consumer;
#ifdef __cplusplus
	MessageQueue(size_t elementSize, size_t queueSize)
	  : elementSize(elementSize), queueSize(queueSize)
	{
	}
#endif
};

_Static_assert(sizeof(struct MessageQueue) % sizeof(void *) == 0,
               "MessageQueue structure must end correctly aligned for storing "
               "capabilities.");

__BEGIN_DECLS

/**
 * Returns the allocation size needed for a queue with the specified number and
 * size of elements.  This can be used to statically allocate queues.
 *
 * Returns the allocation size on success, or `-EINVAL` if the arguments would
 * cause an overflow.
 */
ssize_t __cheri_libcall queue_allocation_size(size_t elementSize,
                                              size_t elementCount);

/**
 * Allocates space for a queue using `heapCapability` and stores a handle to it
 * via `outQueue`.
 *
 * The queue is has space for `elementCount` entries.  Each entry is a fixed
 * size, `elementSize` bytes.
 *
 * Returns 0 on success, `-ENOMEM` on allocation failure, and `-EINVAL` if the
 * arguments are invalid (for example, if the requested number of elements
 * multiplied by the element size would overflow).
 */
int __cheri_libcall queue_create(Timeout              *timeout,
                                 AllocatorCapability   heapCapability,
                                 struct MessageQueue **outQueue,
                                 size_t                elementSize,
                                 size_t                elementCount);

/**
 * Destroys a queue. This wakes up all threads waiting to produce or consume,
 * and makes them fail to acquire the lock, before deallocating the underlying
 * allocation.
 *
 * Returns 0 on success. This can fail only if deallocation would fail and will,
 * in these cases, return the same error codes as `heap_free`.
 *
 * This function will check the heap capability first and so will avoid
 * upgrading the locks if freeing the queue would fail.
 */
int __cheri_libcall queue_destroy(AllocatorCapability  heapCapability,
                                  struct MessageQueue *handle);

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
int __cheri_libcall queue_send(Timeout             *timeout,
                               struct MessageQueue *handle,
                               const void          *src);

/**
 * Receive a message over a queue specified by `handle`.  This expects to be
 * able to copy the number of bytes specified by `elementSize`.  The message is
 * copied to `dst`, which must have sufficient permissions and space to hold
 * the message.
 *
 * Returns 0 on success, `-ETIMEOUT` if the timeout was exhausted, `-EINVAL` on
 * invalid arguments.
 */
int __cheri_libcall queue_receive(Timeout             *timeout,
                                  struct MessageQueue *handle,
                                  void                *dst);

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
int __cheri_libcall queue_items_remaining(struct MessageQueue *handle,
                                          size_t              *items);

/**
 * Allocate a new message queue that is managed by the message queue
 * compartment.  The resulting queue handle (returned in `outQueue`) is a
 * sealed capability to a queue that can be used for both sending and
 * receiving.
 */
int __cheri_compartment("message_queue")
  queue_create_sealed(Timeout            *timeout,
                      AllocatorCapability heapCapability,
                      CHERI_SEALED(struct MessageQueue *) * outQueue,
                      size_t elementSize,
                      size_t elementCount);

/**
 * Destroy a queue handle.  If this is called on a restricted endpoint
 * (returned from `queue_receive_handle_create_sealed` or
 * `queue_send_handle_create_sealed`), this frees only the handle.  If called
 * with the queue handle returned from `queue_create_sealed`, this will destroy
 * the queue.
 */
int __cheri_compartment("message_queue")
  queue_destroy_sealed(Timeout            *timeout,
                       AllocatorCapability heapCapability,
                       CHERI_SEALED(struct MessageQueue *) queueHandle);

/**
 * Send a message via a sealed queue endpoint.  This behaves in the same way as
 * `queue_send`, except that it will return `-EINVAL` if the endpoint is not a
 * valid sending endpoint and may return `-ECOMPARTMENTFAIL` if the queue is
 * destroyed during the call.
 */
int __cheri_compartment("message_queue")
  queue_send_sealed(Timeout *timeout,
                    CHERI_SEALED(struct MessageQueue *) handle,
                    const void *src);

/**
 * Receive a message via a sealed queue endpoint.  This behaves in the same way
 * as `queue_receive`, except that it will return `-EINVAL` if the endpoint is
 * not a valid receiving endpoint and may return `-ECOMPARTMENTFAIL` if the
 * queue is destroyed during the call.
 */
int __cheri_compartment("message_queue")
  queue_receive_sealed(Timeout *timeout,
                       CHERI_SEALED(struct MessageQueue *) handle,
                       void *dst);

/**
 * Returns, via `items`, the number of items in the queue specified by `handle`.
 * Returns 0 on success.
 *
 * This call is intended to be fast and so does minimal checking of arguments.
 * It does not mutate state or acquire any locks and so may return
 * `-ECOMPARTMENTFAIL` for any failure case.
 */
int __cheri_compartment("message_queue")
  queue_items_remaining_sealed(CHERI_SEALED(struct MessageQueue *) handle,
                               size_t *items);

/**
 * Initialise an event waiter source so that it will wait for the queue to be
 * ready to receive.  Note that this is inherently racy because another consumer
 * may drain the queue before this consumer wakes up.
 */
void __cheri_libcall
multiwaiter_queue_receive_init(struct EventWaiterSource *source,
                               struct MessageQueue      *handle);

/**
 * Initialise an event waiter source so that it will wait for the queue to be
 * ready to send.  Note that this is inherently racy because another producer
 * may fill the queue before this consumer wakes up.
 */
void __cheri_libcall
multiwaiter_queue_send_init(struct EventWaiterSource *source,
                            struct MessageQueue      *handle);

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
                                        CHERI_SEALED(struct MessageQueue *)
                                          handle);

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
                                     CHERI_SEALED(struct MessageQueue *)
                                       handle);

/**
 * Convert a queue handle returned from `queue_create_sealed` into one that can
 * be used *only* for receiving.
 *
 * Returns 0 on success and writes the resulting restricted handle via
 * `outHandle`.  Returns `-ENOMEM` on allocation failure or `-EINVAL` if the
 * handle is not valid.
 */
int __cheri_compartment("message_queue")
  queue_receive_handle_create_sealed(struct Timeout     *timeout,
                                     AllocatorCapability heapCapability,
                                     CHERI_SEALED(struct MessageQueue *) handle,
                                     CHERI_SEALED(struct MessageQueue *) *
                                       outHandle);

/**
 * Convert a queue handle returned from `queue_create_sealed` into one that can
 * be used *only* for sending.
 *
 * Returns 0 on success and writes the resulting restricted handle via
 * `outHandle`.  Returns `-ENOMEM` on allocation failure or `-EINVAL` if the
 * handle is not valid.
 */
int __cheri_compartment("message_queue")
  queue_send_handle_create_sealed(struct Timeout     *timeout,
                                  AllocatorCapability heapCapability,
                                  CHERI_SEALED(struct MessageQueue *) handle,
                                  CHERI_SEALED(struct MessageQueue *) *
                                    outHandle);

__END_DECLS

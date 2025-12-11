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
#include <allocator.h>
#include <multiwaiter.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <timeout.h>
#include <token.h>

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

/**
 * Permissions on message queue handles.
 */
enum [[clang::flag_enum]] MessageQueuePermission
{
	/**
	 * This handle may be used to send messages.
	 */
	MessageQueuePermitSend = (1 << 0),
	/**
	 * This handle may be used to receive messages.
	 */
	MessageQueuePermitReceive = (1 << 1),
	/**
	 * This handle may be used to deallocate the queue.
	 */
	MessageQueuePermitDestroy = (1 << 2),
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
 * The queue has space for `elementCount` entries.  Each entry is a fixed
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
 * Stops the queue.  This prevents any future send and receive calls from
 * working, as a prelude to destruction.  Queues allocated by the library
 * should use `queue_destroy` instead of this API, this exists to support
 * deadlock-free deallocation of queues embedded in other structures or where
 * the memory is managed externally in some other way.
 *
 * Returns 0 on success.
 */
int __cheriot_libcall queue_stop(struct MessageQueue *handle);

/**
 * Destroys a queue. This wakes up all threads waiting to produce or consume,
 * and makes them fail to acquire the lock, before deallocating the underlying
 * allocation.
 *
 * Returns 0 on success. This can fail only if deallocation would fail and will,
 * in these cases, return the same error codes as `heap_free`.
 *
 * This function will check the heap capability first and will avoid upgrading
 * the locks if freeing the queue would fail.
 */
int __cheri_libcall queue_destroy(AllocatorCapability  heapCapability,
                                  struct MessageQueue *handle);

/**
 * Send a message to the queue specified by `handle`.  This expects to be able
 * to copy the number of bytes specified by `elementSize` when the queue was
 * created from `src`.
 *
 * Returns 0 on success.  On failure, returns `-ETIMEDOUT` if the timeout was
 * exhausted, `-EINVAL` on invalid arguments.
 *
 * This expects to be called with a valid queue handle.  It does not validate
 * that this is correct.
 */
int __cheri_libcall queue_send(Timeout             *timeout,
                               struct MessageQueue *handle,
                               const void          *src);

/**
 * Send multiple messages to the queue specified by `handle`.  This expects to
 * be able to copy `count` times the number of bytes specified by `elementSize`
 * when the queue was created from `src`.
 *
 * Returns the number of elements sent on success.  On failure, returns
 * `-ETIMEDOUT` if the timeout was exhausted, `-EINVAL` on invalid arguments.
 *
 * This expected to be called with a valid queue handle.  It does not validate
 * that this is correct.
 */
int __cheri_libcall queue_send_multiple(Timeout             *timeout,
                                        struct MessageQueue *handle,
                                        const void          *src,
                                        size_t               count);

/**
 * Receive a message over a queue specified by `handle`.  This expects to be
 * able to copy the number of bytes specified by `elementSize`.  The message is
 * copied to `dst`, which must have sufficient permissions and space to hold
 * the message.
 *
 * Returns 0 on success, `-ETIMEDOUT` if the timeout was exhausted, `-EINVAL` on
 * invalid arguments.
 */
int __cheri_libcall queue_receive(Timeout             *timeout,
                                  struct MessageQueue *handle,
                                  void                *dst);

/**
 * Receive multiple messages to the queue specified by `handle`.  This expects
 * to be able to copy `count` times the number of bytes specified by
 * `elementSize` when the queue was created to `dst`.
 *
 * Returns the number of elements sent on success.  On failure, returns
 * `-ETIMEDOUT` if the timeout was exhausted, `-EINVAL` on invalid arguments.
 *
 * This expected to be called with a valid queue handle.  It does not validate
 * that this is correct.
 */
int __cheri_libcall queue_receive_multiple(Timeout             *timeout,
                                           struct MessageQueue *handle,
                                           void                *dst,
                                           size_t               count);

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
 * Reset a queue to its initial state.
 *
 * Returns 0 on success, `-ETIMEDOUT` if this cannot be done in the available
 * timeout.
 */
int __cheri_libcall queue_reset(Timeout *timeout, struct MessageQueue *queue);

/**
 * Destroy a queue.  This requires a handle with `MessageQueuePermitDestroy`
 * permission.
 *
 * Returns 0 on success, `-EINVAL` if `queueHandle` is not a valid queue handle
 * or lacks the relevant permissions.
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
 * Send multiple messages via a sealed queue endpoint.  This behaves in the
 * same way as `queue_send_multiple`, except that it will return `-EINVAL` if
 * the endpoint is not a valid sending endpoint and may return
 * `-ECOMPARTMENTFAIL` if the queue is destroyed during the call.
 */
int __cheri_compartment("message_queue")
  queue_send_multiple_sealed(Timeout *timeout,
                             CHERI_SEALED(struct MessageQueue *) handle,
                             const void *src,
                             size_t      count);

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
 * Receive multiple messages via a sealed queue endpoint.  This behaves in the
 * same way as `queue_receive_multiple`, except that it will return `-EINVAL` if
 * the endpoint is not a valid receiving endpoint and may return
 * `-ECOMPARTMENTFAIL` if the queue is destroyed during the call.
 */
int __cheri_compartment("message_queue")
  queue_receive_multiple_sealed(Timeout *timeout,
                                CHERI_SEALED(struct MessageQueue *) handle,
                                void  *dst,
                                size_t count);

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
 * Returns the permissions held by this message queue handle.  This is a
 * bitmask of `MessageQueuePermission` values.
 */
static inline int queue_permissions(CHERI_SEALED(struct MessageQueue *) handle)
{
	return token_permissions_get(handle);
}

/**
 * Returns a copy of `handle` with a subset of permissions.  The `permissions`
 * argument is a bitmask of `MessageQueuePermission` values.  The returned
 * handle has only the permissions that are both already present on `handle`
 * and enumerated in `permissions`.
 */
static inline CHERI_SEALED(struct MessageQueue *)
  queue_permissions_and(CHERI_SEALED(struct MessageQueue *) handle,
                        int permissions)
{
	return token_permissions_and(handle, permissions);
}

/**
 * Convert a queue handle returned from `queue_create_sealed` into one that can
 * be used *only* for receiving.
 *
 * Returns 0 on success and writes the resulting restricted handle via
 * `outHandle`.  Returns `-ENOMEM` on allocation failure or `-EINVAL` if the
 * handle is not valid.
 */
static inline int __attribute__((
  deprecated("Restricted handles have been replaced by permissions on queue "
             "capabilities, controlled with queue_permissions_and")))
queue_receive_handle_create_sealed(struct Timeout     *timeout,
                                   AllocatorCapability heapCapability,
                                   CHERI_SEALED(struct MessageQueue *) handle,
                                   CHERI_SEALED(struct MessageQueue *) *
                                     outHandle)
{
	*outHandle = queue_permissions_and(handle, MessageQueuePermitReceive);
	return 0;
}

/**
 * Convert a queue handle returned from `queue_create_sealed` into one that can
 * be used *only* for sending.
 *
 * Returns 0 on success and writes the resulting restricted handle via
 * `outHandle`.  Returns `-ENOMEM` on allocation failure or `-EINVAL` if the
 * handle is not valid.
 */
static inline int __attribute__((
  deprecated("Restricted handles have been replaced by permissions on queue "
             "capabilities, controlled with queue_permissions_and")))
queue_send_handle_create_sealed(struct Timeout     *timeout,
                                AllocatorCapability heapCapability,
                                CHERI_SEALED(struct MessageQueue *) handle,
                                CHERI_SEALED(struct MessageQueue *) * outHandle)
{
	*outHandle = queue_permissions_and(handle, MessageQueuePermitSend);
	return 0;
}

__END_DECLS

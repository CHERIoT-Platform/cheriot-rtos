#include <cheri.hh>
#include <compartment.h>
#include <cstdlib>
#include <errno.h>
#include <locks.hh>
#include <queue.h>
#include <token.h>

using namespace CHERI;

using Debug = ConditionalDebug<false, "Queue compartment">;

namespace
{
	__always_inline SKey receive_key()
	{
		return STATIC_SEALING_TYPE(ReceiveHandle);
	}
	__always_inline SKey send_key()
	{
		return STATIC_SEALING_TYPE(SendHandle);
	}

	struct QueueEndpoint
	{
		QueueHandle handle;
		void       *allocation;
		// Lock that protects against double free.
		FlagLockPriorityInherited lock;
	};

} // namespace

int queue_create_sealed(Timeout            *timeout,
                        struct SObjStruct  *heapCapability,
                        struct SObjStruct **outQueueSend,
                        struct SObjStruct **outQueueReceive,
                        size_t              elementSize,
                        size_t              elementCount)
{
	if (heap_address_is_valid(timeout) ||
	    !check_pointer<PermissionSet{Permission::Load, Permission::Store}>(
	      timeout))
	{
		return -EPERM;
	}

	// Allocate the queue endpoints
	auto [send, sendSealed] =
	  token_allocate<QueueEndpoint>(timeout, heapCapability, send_key());
	if (!send)
	{
		return timeout->may_block() ? -ENOMEM : -ETIMEDOUT;
	}
	auto [receive, receiveSealed] =
	  token_allocate<QueueEndpoint>(timeout, heapCapability, receive_key());
	if (!receive)
	{
		token_obj_destroy(heapCapability, send_key(), sendSealed);
		return timeout->may_block() ? -ENOMEM : -ETIMEDOUT;
	}

	// Bidirectional queue handle
	QueueHandle handle;
	// The pointer to the queue that is used when freeing
	void *freeBuffer;
	// Allocate the queue object
	int ret = queue_create(
	  timeout, heapCapability, &handle, &freeBuffer, elementSize, elementCount);
	if (ret != 0)
	{
		token_obj_destroy(heapCapability, send_key(), sendSealed);
		token_obj_destroy(heapCapability, receive_key(), receiveSealed);
		return ret;
	}

	send->handle        = queue_make_send_handle(handle);
	send->allocation    = freeBuffer;
	receive->handle     = queue_make_receive_handle(handle);
	receive->allocation = freeBuffer;
	// Add a second claim on the buffer so that we can free the queue by freeing
	// it twice, once in each endpoint.
	heap_claim(heapCapability, freeBuffer);

	if (int claimed = heap_claim_fast(timeout, outQueueSend, outQueueReceive);
	    claimed != 0)
	{
		return claimed;
	}
	if (!check_pointer<PermissionSet{Permission::Load,
	                                 Permission::Store,
	                                 Permission::LoadStoreCapability}>(
	      outQueueReceive, sizeof(void *)) ||
	    !check_pointer<PermissionSet{Permission::Load,
	                                 Permission::Store,
	                                 Permission::LoadStoreCapability}>(
	      outQueueSend, sizeof(void *)))
	{
		// Free twice because we claimed it once in addition to the original
		// allocation.
		heap_free(heapCapability, freeBuffer);
		heap_free(heapCapability, freeBuffer);
		return -EPERM;
	}
	*outQueueSend    = sendSealed;
	*outQueueReceive = receiveSealed;
	return 0;
}

int queue_destroy_sealed(Timeout           *timeout,
                         struct SObjStruct *heapCapability,
                         struct SObjStruct *queueHandle)
{
	Debug::log("Destroying queue {}", queueHandle);
	auto  token = receive_key();
	auto *end   = token_unseal(token, Sealed<QueueEndpoint>{queueHandle});
	// This function takes either endpoint, so we need to try unsealing with
	// both keys.
	if (!end)
	{
		token = send_key();
		end   = token_unseal(token, Sealed<QueueEndpoint>{queueHandle});
	}
	if (!end)
	{
		return -EINVAL;
	}
	// Don't bother with a lock guard: we will destroy this lock if we reach the
	// end. If we lose a race here, this will trap and we will implicitly return
	// `-ECOMPARTMENTFAIL`.
	if (!end->lock.try_lock(timeout))
	{
		return -ETIMEDOUT;
	}
	if (heap_free(heapCapability, end->allocation) != 0)
	{
		end->lock.unlock();
		return -EPERM;
	}
	token_obj_destroy(heapCapability, token, queueHandle);
	return 0;
}

int queue_send_sealed(Timeout           *timeout,
                      struct SObjStruct *handle,
                      const void        *src)
{
	auto *end = token_unseal(send_key(), Sealed<QueueEndpoint>{handle});
	// If we failed to unseal, or if the timeout is on the heap, invalid
	// argument error.
	if (!end || heap_address_is_valid(timeout))
	{
		return -EINVAL;
	}
	return queue_send(timeout, &end->handle, src);
}

int queue_receive_sealed(Timeout *timeout, struct SObjStruct *handle, void *dst)
{
	auto *end = token_unseal(receive_key(), Sealed<QueueEndpoint>{handle});
	// If we failed to unseal, or if the timeout is on the heap, invalid
	// argument error.
	if (!end || heap_address_is_valid(timeout))
	{
		return -EINVAL;
	}
	return queue_receive(timeout, &end->handle, dst);
}

int multiwaiter_queue_receive_init_sealed(struct EventWaiterSource *source,
                                          struct SObjStruct        *handle)
{
	auto *end = token_unseal(receive_key(), Sealed<QueueEndpoint>{handle});
	if (!end)
	{
		return -EINVAL;
	}
	multiwaiter_queue_receive_init(source, &end->handle);
	return 0;
}

int multiwaiter_queue_send_init_sealed(struct EventWaiterSource *source,
                                       struct SObjStruct        *handle)
{
	auto *end = token_unseal(send_key(), Sealed<QueueEndpoint>{handle});
	if (!end)
	{
		return -EINVAL;
	}
	multiwaiter_queue_send_init(source, &end->handle);
	return 0;
}

int queue_items_remaining_sealed(struct SObjStruct *queueHandle, size_t *items)
{
	auto *end = token_unseal(receive_key(), Sealed<QueueEndpoint>{queueHandle});
	// This function takes either endpoint, so we need to try unsealing with
	// both keys.
	if (!end)
	{
		end = token_unseal(send_key(), Sealed<QueueEndpoint>{queueHandle});
	}
	queue_items_remaining(&end->handle, items);
	return 0;
}

#include <cheri.hh>
#include <compartment.h>
#include <cstdlib>
#include <errno.h>
#include <locks.hh>
#include <queue.h>
#include <token.h>

using namespace CHERI;

using Debug = ConditionalDebug<false, "MessageQueue compartment">;

namespace
{
	__always_inline SKey handle_key()
	{
		return STATIC_SEALING_TYPE(MessageQueueHandle);
	}
	__always_inline SKey receive_key()
	{
		return STATIC_SEALING_TYPE(ReceiveHandle);
	}
	__always_inline SKey send_key()
	{
		return STATIC_SEALING_TYPE(SendHandle);
	}

	/**
	 * Wrapper used for restricted endpoints.  This is used to provide
	 * capabilities that allow only sending or receiving.  Instances of this
	 * will be sealed with either `send_key()` or `receive_key()` to define
	 * their types.
	 */
	struct RestrictedEndpoint
	{
		MessageQueue *handle;
	};

	/**
	 * Unseal something that is either a queue handle or a restricted endpoint
	 * with the specified key.
	 */
	MessageQueue *unseal(SKey key, SObj handle)
	{
		MessageQueue *queue = nullptr;
		if (auto *unsealed =
		      token_unseal(key, Sealed<RestrictedEndpoint>{handle}))
		{
			queue = unsealed->handle;
		}
		else if (auto *unsealed =
		           token_unseal(handle_key(), Sealed<MessageQueue>{handle}))
		{
			queue = unsealed;
		}
		return queue;
	}

} // namespace

int queue_create_sealed(Timeout            *timeout,
                        struct SObjStruct  *heapCapability,
                        struct SObjStruct **outQueue,
                        size_t              elementSize,
                        size_t              elementCount)
{
	ssize_t allocSize = queue_allocation_size(elementSize, elementCount);
	if (allocSize < 0)
	{
		return -EINVAL;
	}

	void *unsealed = nullptr;
	// Allocate the space for the queue.
	auto sealed = token_sealed_unsealed_alloc(
	  timeout, heapCapability, handle_key(), allocSize, &unsealed);
	if (!unsealed)
	{
		return -ENOMEM;
	}

	new (unsealed) MessageQueue(elementSize, elementCount);
	*outQueue = sealed;
	return 0;
}

int queue_destroy_sealed(Timeout           *timeout,
                         struct SObjStruct *heapCapability,
                         struct SObjStruct *queueHandle)
{
	if (token_obj_unseal(handle_key(), queueHandle) != nullptr)
	{
		token_obj_destroy(heapCapability, handle_key(), queueHandle);
		return 0;
	}
	if (token_obj_unseal(send_key(), queueHandle) != nullptr)
	{
		token_obj_destroy(heapCapability, send_key(), queueHandle);
		return 0;
	}
	if (token_obj_unseal(receive_key(), queueHandle) != nullptr)
	{
		token_obj_destroy(heapCapability, receive_key(), queueHandle);
		return 0;
	}
	return -EINVAL;
}

int queue_send_sealed(Timeout           *timeout,
                      struct SObjStruct *handle,
                      const void        *src)
{
	MessageQueue *queue = unseal(send_key(), handle);
	if (!queue || !check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return queue_send(timeout, queue, src);
}

int queue_receive_sealed(Timeout *timeout, struct SObjStruct *handle, void *dst)
{
	MessageQueue *queue = unseal(receive_key(), handle);
	if (!queue || !check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return queue_receive(timeout, queue, dst);
}

int multiwaiter_queue_receive_init_sealed(struct EventWaiterSource *source,
                                          struct SObjStruct        *handle)
{
	MessageQueue *queue = unseal(receive_key(), handle);
	if (!queue)
	{
		return -EINVAL;
	}
	multiwaiter_queue_receive_init(source, queue);
	return 0;
}

int multiwaiter_queue_send_init_sealed(struct EventWaiterSource *source,
                                       struct SObjStruct        *handle)
{
	MessageQueue *queue = unseal(send_key(), handle);
	if (!queue)
	{
		return -EINVAL;
	}
	multiwaiter_queue_send_init(source, queue);
	return 0;
}

int queue_items_remaining_sealed(struct SObjStruct *handle, size_t *items)
{
	MessageQueue *queue = unseal(send_key(), handle);
	// This function takes either endpoint, so we need to try unsealing with
	// both keys.
	if (!queue)
	{
		if (auto *unsealed =
		      token_unseal(receive_key(), Sealed<RestrictedEndpoint>{handle}))
		{
			queue = unsealed->handle;
		}
	}
	if (!queue)
	{
		return -EINVAL;
	}
	queue_items_remaining(queue, items);
	return 0;
}

int queue_receive_handle_create_sealed(struct Timeout     *timeout,
                                       struct SObjStruct  *heapCapability,
                                       struct SObjStruct  *handle,
                                       struct SObjStruct **outHandle)
{
	MessageQueue *queue =
	  token_unseal(handle_key(), Sealed<MessageQueue>(handle));
	if (!queue)
	{
		return -EINVAL;
	}
	auto [unsealed, sealed] = token_allocate<RestrictedEndpoint>(
	  timeout, heapCapability, receive_key());
	if (!unsealed)
	{
		return -ENOMEM;
	}
	unsealed->handle = queue;
	*outHandle       = sealed;
	return 0;
}

int queue_send_handle_create_sealed(struct Timeout     *timeout,
                                    struct SObjStruct  *heapCapability,
                                    struct SObjStruct  *handle,
                                    struct SObjStruct **outHandle)
{
	MessageQueue *queue =
	  token_unseal(handle_key(), Sealed<MessageQueue>(handle));
	if (!queue)
	{
		return -EINVAL;
	}
	auto [unsealed, sealed] =
	  token_allocate<RestrictedEndpoint>(timeout, heapCapability, send_key());
	if (!unsealed)
	{
		return -ENOMEM;
	}
	unsealed->handle = queue;
	*outHandle       = sealed;
	return 0;
}

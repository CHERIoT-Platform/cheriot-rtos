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
	MessageQueue *unseal(SKey key, CHERI_SEALED(MessageQueue *) handle)
	{
		MessageQueue *queue = nullptr;
		// TODO: The cast expression below is overcomplicated
		// (RestrictedEndpoint is specified twice) so that it works with both
		// the old SObj model and the new type-system-exposed sealing model.
		if (auto *unsealed = token_unseal(
		      key,
		      Sealed<RestrictedEndpoint>{
		        reinterpret_cast<CHERI_SEALED(RestrictedEndpoint *)>(handle)}))
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
                        AllocatorCapability heapCapability,
                        CHERI_SEALED(MessageQueue *) * outQueue,
                        size_t elementSize,
                        size_t elementCount)
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
	*outQueue = static_cast<CHERI_SEALED(MessageQueue *)>(sealed);
	return 0;
}

int queue_destroy_sealed(Timeout            *timeout,
                         AllocatorCapability heapCapability,
                         CHERI_SEALED(MessageQueue *) queueHandle)
{
	if (token_obj_unseal(handle_key(), queueHandle) != nullptr)
	{
		return token_obj_destroy(heapCapability, handle_key(), queueHandle);
	}
	if (token_obj_unseal(send_key(), queueHandle) != nullptr)
	{
		return token_obj_destroy(heapCapability, send_key(), queueHandle);
	}
	if (token_obj_unseal(receive_key(), queueHandle) != nullptr)
	{
		return token_obj_destroy(heapCapability, receive_key(), queueHandle);
	}
	return -EINVAL;
}

int queue_send_sealed(Timeout *timeout,
                      CHERI_SEALED(MessageQueue *) handle,
                      const void *src)
{
	MessageQueue *queue = unseal(send_key(), handle);
	if (!queue || !check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return queue_send(timeout, queue, src);
}

int queue_send_multiple_sealed(Timeout *timeout,
                               CHERI_SEALED(MessageQueue *) handle,
                               const void *src,
                               size_t      count)
{
	MessageQueue *queue = unseal(send_key(), handle);
	if (!queue || !check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return queue_send_multiple(timeout, queue, src, count);
}

int queue_receive_sealed(Timeout *timeout,
                         CHERI_SEALED(MessageQueue *) handle,
                         void *dst)
{
	MessageQueue *queue = unseal(receive_key(), handle);
	if (!queue || !check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return queue_receive(timeout, queue, dst);
}

int queue_receive_multiple_sealed(Timeout *timeout,
                                  CHERI_SEALED(MessageQueue *) handle,
                                  void  *dst,
                                  size_t count)
{
	MessageQueue *queue = unseal(receive_key(), handle);
	if (!queue || !check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return queue_receive_multiple(timeout, queue, dst, count);
}

int multiwaiter_queue_receive_init_sealed(struct EventWaiterSource *source,
                                          CHERI_SEALED(MessageQueue *) handle)
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
                                       CHERI_SEALED(MessageQueue *) handle)
{
	MessageQueue *queue = unseal(send_key(), handle);
	if (!queue)
	{
		return -EINVAL;
	}
	multiwaiter_queue_send_init(source, queue);
	return 0;
}

int queue_items_remaining_sealed(CHERI_SEALED(MessageQueue *) handle,
                                 size_t *items)
{
	MessageQueue *queue = unseal(send_key(), handle);
	// This function takes either endpoint, so we need to try unsealing with
	// both keys.
	if (!queue)
	{
		queue = unseal(receive_key(), handle);
	}
	if (!queue)
	{
		return -EINVAL;
	}
	queue_items_remaining(queue, items);
	return 0;
}

namespace
{
	int queue_handle_create_sealed(struct Timeout     *timeout,
	                               AllocatorCapability heapCapability,
	                               CHERI_SEALED(MessageQueue *) handle,
	                               CHERI_SEALED(MessageQueue *) * outHandle,
	                               SKey sealingKey)
	{
		MessageQueue *queue =
		  token_unseal(handle_key(), Sealed<MessageQueue>(handle));
		if (!queue)
		{
			return -EINVAL;
		}
		auto [unsealed, sealed] = token_allocate<RestrictedEndpoint>(
		  timeout, heapCapability, sealingKey);
		if (!sealed.is_valid())
		{
			return -ENOMEM;
		}
		unsealed->handle = queue;
		*outHandle =
		  reinterpret_cast<CHERI_SEALED(MessageQueue *)>(sealed.get());
		return 0;
	}
} // namespace

int queue_receive_handle_create_sealed(struct Timeout     *timeout,
                                       AllocatorCapability heapCapability,
                                       CHERI_SEALED(MessageQueue *) handle,
                                       CHERI_SEALED(MessageQueue *) * outHandle)
{
	return queue_handle_create_sealed(
	  timeout, heapCapability, handle, outHandle, receive_key());
}

int queue_send_handle_create_sealed(struct Timeout     *timeout,
                                    AllocatorCapability heapCapability,
                                    CHERI_SEALED(MessageQueue *) handle,
                                    CHERI_SEALED(MessageQueue *) * outHandle)
{
	return queue_handle_create_sealed(
	  timeout, heapCapability, handle, outHandle, send_key());
}

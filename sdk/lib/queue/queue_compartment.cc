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
	__always_inline TokenKey handle_key()
	{
		return STATIC_SEALING_TYPE(MessageQueueHandle);
	}

	template<MessageQueuePermission... Permissions>
	__always_inline bool has_permissions(CHERI_SEALED(MessageQueue *) handle)
	{
		static const constinit int PermissionsMask = []() {
			std::tuple permissions    = std::make_tuple(Permissions...);
			int        permissionMask = 0;
			std::for_each(permissions, [&](MessageQueuePermission permission) {
				permissionMask |= permission;
			});
			return permissionMask;
		};
		return ((queue_permissions(handle) & PermissionsMask) ==
		        PermissionsMask);
	}

	/**
	 * Unseal a queue handle if it has all the given permissions.  Returns the
	 * unsealed handle or null if the handle is invalid or lacks the relevant
	 * permissions.
	 */
	MessageQueue *unseal(CHERI_SEALED(MessageQueue *) handle, int permissions)
	{
		if ((queue_permissions(handle) & permissions) != permissions)
		{
			return nullptr;
		}
		return token_unseal(handle_key(), Sealed<MessageQueue>{handle});
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
	if (MessageQueue *queue = unseal(queueHandle, MessageQueuePermitDestroy))
	{
		if (token_obj_can_destroy(heapCapability, handle_key(), queueHandle) !=
		    0)
		{
			return -EPERM;
		}
		queue_stop(queue);
		return token_obj_destroy(heapCapability, handle_key(), queueHandle);
	}
	return -EINVAL;
}

int queue_send_sealed(Timeout *timeout,
                      CHERI_SEALED(MessageQueue *) handle,
                      const void *src)
{
	MessageQueue *queue = unseal(handle, MessageQueuePermitSend);
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
	MessageQueue *queue = unseal(handle, MessageQueuePermitSend);
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
	MessageQueue *queue = unseal(handle, MessageQueuePermitReceive);
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
	MessageQueue *queue = unseal(handle, MessageQueuePermitReceive);
	if (!queue || !check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return queue_receive_multiple(timeout, queue, dst, count);
}

int multiwaiter_queue_receive_init_sealed(struct EventWaiterSource *source,
                                          CHERI_SEALED(MessageQueue *) handle)
{
	MessageQueue *queue = unseal(handle, MessageQueuePermitReceive);
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
	MessageQueue *queue = unseal(handle, MessageQueuePermitSend);
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
	MessageQueue *queue = unseal(handle, 0);
	if ((queue_permissions(handle) &
	     (MessageQueuePermitSend | MessageQueuePermitReceive)) == 0)
	{
		return -EPERM;
	}
	if (!queue)
	{
		return -EINVAL;
	}
	queue_items_remaining(queue, items);
	return 0;
}

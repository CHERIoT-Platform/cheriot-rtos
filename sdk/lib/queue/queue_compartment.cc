#include <allocator.h>
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
	ErrorOr<MessageQueue> unseal(CHERI_SEALED(MessageQueue *) handle,
	                             int permissions)
	{
		if ((queue_permissions(handle) & permissions) != permissions)
		{
			return -EPERM;
		}
		auto queue = token_unseal(handle_key(), Sealed<MessageQueue>{handle});
		if (queue)
		{
			return queue;
		}
		return -EINVAL;
	}

	/**
	 * Helper to run a function with the unsealed queue if the handle has the
	 * given permissions.
	 *
	 * Returns either:
	 *  - -EPERM if the handle does not have all the requested permissions
	 *  - -EINVAL if the handle cannot be unsealed
	 *  - The return value of `fn` otherwise
	 */
	int with_unsealed(CHERI_SEALED(MessageQueue *) handle,
	                  int    permissions,
	                  auto &&fn)
	{
		return unseal(handle, permissions).and_then(fn);
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
	// timeout unused?!
	return with_unsealed(
	  queueHandle, MessageQueuePermitDestroy, [&](MessageQueue *queue) {
		  if (token_obj_can_destroy(
		        heapCapability, handle_key(), queueHandle) != 0)
		  {
			  return -EPERM;
		  }

		  queue_stop(queue);

		  return token_obj_destroy(heapCapability, handle_key(), queueHandle);
	  });
}

int queue_send_sealed(Timeout *timeout,
                      CHERI_SEALED(MessageQueue *) handle,
                      const void *src)
{
	if (!check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return with_unsealed(handle, MessageQueuePermitSend, [&](MessageQueue *q) {
		return queue_send(timeout, q, src);
	});
}

int queue_send_multiple_sealed(Timeout *timeout,
                               CHERI_SEALED(MessageQueue *) handle,
                               const void *src,
                               size_t      count)
{
	if (!check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return with_unsealed(handle, MessageQueuePermitSend, [&](MessageQueue *q) {
		return queue_send_multiple(timeout, q, src, count);
	});
}

int queue_receive_sealed(Timeout *timeout,
                         CHERI_SEALED(MessageQueue *) handle,
                         void *dst)
{
	if (!check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return with_unsealed(
	  handle, MessageQueuePermitReceive, [&](MessageQueue *q) {
		  return queue_receive(timeout, q, dst);
	  });
}

int queue_receive_multiple_sealed(Timeout *timeout,
                                  CHERI_SEALED(MessageQueue *) handle,
                                  void  *dst,
                                  size_t count)
{
	if (!check_timeout_pointer(timeout))
	{
		return -EINVAL;
	}
	return with_unsealed(
	  handle, MessageQueuePermitReceive, [&](MessageQueue *q) {
		  return queue_receive_multiple(timeout, q, dst, count);
	  });
}

int multiwaiter_queue_receive_init_sealed(struct EventWaiterSource *source,
                                          CHERI_SEALED(MessageQueue *) handle)
{
	return with_unsealed(
	  handle, MessageQueuePermitReceive, [&](MessageQueue *q) {
		  multiwaiter_queue_receive_init(source, q);
		  return 0;
	  });
}

int multiwaiter_queue_send_init_sealed(struct EventWaiterSource *source,
                                       CHERI_SEALED(MessageQueue *) handle)
{
	return with_unsealed(
	  handle, MessageQueuePermitSend, [&](MessageQueue *queue) {
		  multiwaiter_queue_send_init(source, queue);
		  return 0;
	  });
}

int queue_items_remaining_sealed(CHERI_SEALED(MessageQueue *) handle,
                                 size_t *items)
{
	if ((queue_permissions(handle) &
	     (MessageQueuePermitSend | MessageQueuePermitReceive)) == 0)
	{
		return -EPERM;
	}
	return with_unsealed(handle, 0, [&](MessageQueue *queue) {
		queue_items_remaining(queue, items);
		return 0;
	});
}

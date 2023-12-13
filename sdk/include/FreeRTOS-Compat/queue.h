#pragma once
#include "FreeRTOS.h"
#include <queue.h>

#define errQUEUE_EMPTY ((BaseType_t)0)
#define errQUEUE_FULL ((BaseType_t)0)

/**
 * Queue handle.  This is used to reference queues in the API functions.
 */
typedef struct
{
	/**
	 * The real handle, holds pointers to the relevant elements of the queue.
	 */
	struct QueueHandle handle;
	/**
	 * The pointer used to free the queue.
	 */
	void *freePointer;
} * QueueHandle_t;

/**
 * Receive a message on a queue.  The message is received into `buffer`, which
 * must be large enough to accommodate a message of the size passed to
 * `xQueueCreate`.  This blocks for up to `waitTicks` ticks if the queue is
 * empty.
 *
 * Returns `pdPASS` if a message was received, or `errQUEUE_EMPTY` if the queue
 * is empty and no message arrived (or was not collected by another
 * higher-priority thread) for the duration of the call.
 */
static inline BaseType_t
xQueueReceive(QueueHandle_t queueHandle, void *buffer, TickType_t waitTicks)
{
	struct Timeout timeout = {0, waitTicks};
	int            rv = queue_receive(&timeout, &queueHandle->handle, buffer);

	if (rv == 0)
		return pdPASS;
	return errQUEUE_EMPTY;
}

/**
 * Send a message to the queue.  Blocks for up to `waitTicks` ticks if the
 * queue is full.  The message is copied from `buffer` which must be large
 * enough to accommodate a message of the size passed to `xQueueCreate`.
 *
 * returns `pdPASS` if the message was sent, or `errQUEUE_FULL` if the queue
 * remained full for the duration of the call.
 */
static inline BaseType_t xQueueSendToBack(QueueHandle_t queueHandle,
                                          const void   *buffer,
                                          TickType_t    waitTicks)
{
	struct Timeout timeout = {0, waitTicks};
	int            rv      = queue_send(&timeout, &queueHandle->handle, buffer);

	if (rv == 0)
		return pdPASS;
	return errQUEUE_FULL;
}

/**
 * Send a message to the queue from an ISR.  We do not allow running code from
 * ISRs and so this behaves like a non-blocking `xQueueSendToBack`.
 *
 * The `pxHigherPriorityTaskWoken` parameter is used to return whether a yield
 * is necessary.  A yield is never necessary in this implementation and so this
 * is unconditionally given a value of `pdFALSE`.
 */
static inline BaseType_t
xQueueSendToBackFromISR(QueueHandle_t queueHandle,
                        const void   *buffer,
                        BaseType_t   *pxHigherPriorityTaskWoken)
{
	*pxHigherPriorityTaskWoken = pdFALSE;
	return xQueueSendToBack(queueHandle, buffer, 0);
}

/**
 * Create a queue that can store `uxQueueLength` messages of size `uxItemSize`.
 * Returns NULL if queue creation failed, false otherwise.
 */
static inline QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength,
                                         UBaseType_t uxItemSize)
{
	QueueHandle_t  ret;
	struct Timeout timeout = {0, UnlimitedTimeout};
	ret                    = (QueueHandle_t)malloc(sizeof(struct QueueHandle));
	if (!ret)
	{
		return NULL;
	}
	int rc = queue_create(&timeout,
	                      MALLOC_CAPABILITY,
	                      &ret->handle,
	                      &ret->freePointer,
	                      uxItemSize,
	                      uxQueueLength);
	if (rc)
	{
		free(ret);
		return NULL;
	}
	return ret;
}

/**
 * Delete a queue.  This frees the memory associated with the queue.
 */
static inline void vQueueDelete(QueueHandle_t xQueue)
{
	free(xQueue->freePointer);
	free(xQueue);
}

/**
 * Return the number of messages waiting in a queue.
 *
 * Note: This is inherently racy and should not be used for anything other than
 * debugging.
 */
static inline UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue)
{
	size_t ret;
	int    rv = queue_items_remaining(&xQueue->handle, &ret);

	assert(rv == 0);

	return ret;
}

#pragma once
#include "FreeRTOS.h"
#include <queue.h>

/**
 * Stream handle.  This is used to reference streams in the API functions.
 */
typedef struct MessageQueue *StreamBufferHandle_t;

#ifndef CHERIOT_NO_AMBIENT_MALLOC
/**
 * Create a stream buffer that can store `xBufferSizeBytes` bytes.
 *
 * Returns NULL if queue creation failed, false otherwise.
 */
static inline StreamBufferHandle_t
xStreamBufferCreate(size_t xBufferSizeBytes, size_t xTriggerLevelBytes)
{
	(void)xTriggerLevelBytes;
	StreamBufferHandle_t ret     = NULL;
	struct Timeout       timeout = {0, UnlimitedTimeout};
	int                  rc =
	  queue_create(&timeout, MALLOC_CAPABILITY, &ret, 1, xBufferSizeBytes);
	return ret;
}

/**
 * Destroy a queue.
 *
 * Note that, unlike the underlying CHERIoT RTOS API, this has no mechanism to
 * signal failure.  If memory deallocation fails (for example, as a result of
 * insufficient stack or trusted-stack space, this API may leak the queue.
 */
static inline void vStreamBufferDelete(StreamBufferHandle_t xStreamBuffer)
{
	struct Timeout timeout = {0, UnlimitedTimeout};
	(void)queue_destroy(MALLOC_CAPABILITY, xStreamBuffer);
}
#endif

/**
 * Sends `xDataLengthBytes` of data from `pvTxData` to the stream
 * `xStreamBuffer`.
 *
 * Returns the number of bytes read or zero in case of an error.
 * Unlike the underlying CHERIoT RTOS API, this API has no way of signalling
 * specific errors so all errors are reported as zero return values.
 */
static inline size_t xStreamBufferSend(StreamBufferHandle_t xStreamBuffer,
                                       const void          *pvTxData,
                                       size_t               xDataLengthBytes,
                                       TickType_t           waitTicks)
{
	struct Timeout timeout = {0, waitTicks};
	int            rv =
	  queue_send_multiple(&timeout, xStreamBuffer, pvTxData, xDataLengthBytes);
	if (rv < 0)
	{
		return 0;
	}
	return rv;
}

/*
 * Send to the stream from an ISR.  We do not allow running code from ISRs and
 * so this behaves like a non-blocking `xStreamBufferSend`.
 *
 * The `pxHigherPriorityTaskWoken` parameter is used to return whether a yield
 * is necessary.  A yield is never necessary in this implementation and so this
 * is unconditionally given a value of `pdFALSE`.
 */
static inline size_t
xStreamBufferSendFromISR(StreamBufferHandle_t xStreamBuffer,
                         const void          *pvTxData,
                         size_t               xDataLengthBytes,
                         BaseType_t          *pxHigherPriorityTaskWoken)
{
	*pxHigherPriorityTaskWoken = pdFALSE;
	return xStreamBufferSend(xStreamBuffer, pvTxData, xDataLengthBytes, 0);
}

/**
 * Receive up to `xBufferLengthBytes` bytes into `pvRxData` from the stream
 * given by `xStreamBuffer`.
 *
 * Returns the number of bytes read, this may be zero in case of an error.
 * Unlike the underlying CHERIoT RTOS API, this API has no way of signalling
 * specific errors so all errors are reported as zero return values.
 */
static inline size_t xStreamBufferReceive(StreamBufferHandle_t xStreamBuffer,
                                          void                *pvRxData,
                                          size_t     xBufferLengthBytes,
                                          TickType_t xTicksToWait)
{
	struct Timeout timeout = {0, xTicksToWait};
	int            rv      = queue_receive_multiple(
      &timeout, xStreamBuffer, pvRxData, xBufferLengthBytes);
	if (rv < 0)
	{
		return 0;
	}
	return rv;
}

/*
 * Receive from the stream from an ISR.  We do not allow running code from ISRs
 * and so this behaves like a non-blocking `xStreamBufferReceive`.
 *
 * The `pxHigherPriorityTaskWoken` parameter is used to return whether a yield
 * is necessary.  A yield is never necessary in this implementation and so this
 * is unconditionally given a value of `pdFALSE`.
 */
static inline size_t
xStreamBufferReceiveFromISR(StreamBufferHandle_t xStreamBuffer,
                            void                *pvRxData,
                            size_t               xBufferLengthBytes,
                            BaseType_t          *pxHigherPriorityTaskWoken)
{
	*pxHigherPriorityTaskWoken = pdFALSE;
	return xStreamBufferSend(xStreamBuffer, pvRxData, xBufferLengthBytes, 0);
}

/**
 * Returns the amount of data in a stream, in bytes.
 *
 * This API is intrinsically racy because more data can be sent to the queue in
 * between calling this function and acting on the result.
 *
 * Note that, unlike FreeRTOS streams, CHERIoT RTOS message queues are thread
 * safe and so this amount can also *decrease* if another thread receives from
 * this stream.
 */
static inline size_t
xStreamBufferBytesAvailable(StreamBufferHandle_t xStreamBuffer)
{
	size_t outItems;
	// Stream buffers use an item size of one and so the number of items and the
	// number of bytes is the same.
	(void)queue_items_remaining(xStreamBuffer, &outItems);
	return outItems;
}

/**
 * Returns the amount of space available in a stream, in bytes.
 *
 * This API is intrinsically racy because more data can be read from the queue
 * in between calling this function and acting on the result.
 *
 * Note that, unlike FreeRTOS streams, CHERIoT RTOS message queues are thread
 * safe and so this amount can also *decrease* if another thread sends over
 * this stream.
 */
static inline size_t
xStreamBufferSpacesAvailable(StreamBufferHandle_t xStreamBuffer)
{
	size_t outItems;
	// Stream buffers use an item size of one and so the number of items and the
	// number of bytes is the same.
	(void)queue_items_remaining(xStreamBuffer, &outItems);
	return xStreamBuffer->queueSize - outItems;
}

/**
 * Updates the trigger level of the stream.
 *
 * Trigger levels are not currently supported by CHERIoT RTOS.
 */
__attribute__((
  deprecated("Trigger levels are not currently supported in the FreeRTOS "
             "streams compatibility layer"))) static inline BaseType_t
xStreamBufferSetTriggerLevel(StreamBufferHandle_t xStreamBuffer,
                             size_t               xTriggerLevel)
{
	return pdFALSE;
}

/**
 * Returns `pdTRUE` if the stream is empty, `pdFALSE` otherwise.
 *
 * Note, this API is inherently racy.
 */
static inline BaseType_t
xStreamBufferIsEmpty(StreamBufferHandle_t xStreamBuffer)
{
	return xStreamBufferBytesAvailable(xStreamBuffer) == 0;
}

/**
 * Returns `pdTRUE` if the stream is full, `pdFALSE` otherwise.
 *
 * Note, this API is inherently racy.
 */
static inline BaseType_t xStreamBufferIsFull(StreamBufferHandle_t xStreamBuffer)
{
	return xStreamBufferSpacesAvailable(xStreamBuffer) == 0;
}

/**
 * Reset the stream, unless another thread is currently active using it.
 *
 * This differs slightly from the FreeRTOS implementation, threads blocked
 * waiting to acquire the endpoint locks do not prevent this operation and any
 * threads waiting to send will become unblocked.
 */
static inline BaseType_t xStreamBufferReset(StreamBufferHandle_t xStreamBuffer)
{
	struct Timeout timeout = {0, 0};
	return queue_reset(&timeout, xStreamBuffer) == 0;
}

/**
 * Reset a stream from an ISR.  We do not allow running code from
 * ISRs and so this behaves like a non-blocking `xStreamBufferReset`.
 */
static inline BaseType_t
xStreamBufferResetFromISR(StreamBufferHandle_t xStreamBuffer)
{
	return xStreamBufferReset(xStreamBuffer);
}

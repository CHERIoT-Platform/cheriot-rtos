#ifndef _FREERTOS_COMPAT_H_
#define _FREERTOS_COMPAT_H_

#define INC_FREERTOS_H
#define INC_TASK_H

#include <assert.h>
#include <errno.h>
#include <event.h>
#include <tick_macros.h>
#include <queue.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <thread.h>

typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint16_t TaskHandle_t;
typedef void* QueueHandle_t;
typedef TickType_t EventBits_t;
typedef void* EventGroupHandle_t;
typedef QueueHandle_t SemaphoreHandle_t;
typedef uint64_t TimeOut_t;

#define pdFALSE (BaseType_t)0
#define pdTRUE (BaseType_t)1
#define pdFAIL pdFALSE
#define pdPASS pdTRUE
#define errQUEUE_EMPTY ((BaseType_t)0)
#define errQUEUE_FULL ((BaseType_t)0)

#define configASSERT(x) assert((x))
#define configASSERT_DEFINED 1
#define configTICK_RATE_HZ TICK_RATE_HZ
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS MS_PER_TICK
#define PRIVILEGED_FUNCTION
#define mtCOVERAGE_TEST_DELAY()
#define mtCOVERAGE_TEST_MARKER()
#define configUSE_PREEMPTION 1

/* The following errno values are used by FreeRTOS+ components, not FreeRTOS
 * itself. */
#define pdFREERTOS_ERRNO_NONE             0   /* No errors */
#define pdFREERTOS_ERRNO_ENOENT           2   /* No such file or directory */
#define pdFREERTOS_ERRNO_EINTR            4   /* Interrupted system call */
#define pdFREERTOS_ERRNO_EIO              5   /* I/O error */
#define pdFREERTOS_ERRNO_ENXIO            6   /* No such device or address */
#define pdFREERTOS_ERRNO_EBADF            9   /* Bad file number */
#define pdFREERTOS_ERRNO_EAGAIN           11  /* No more processes */
#define pdFREERTOS_ERRNO_EWOULDBLOCK      11  /* Operation would block */
#define pdFREERTOS_ERRNO_ENOMEM           12  /* Not enough memory */
#define pdFREERTOS_ERRNO_EACCES           13  /* Permission denied */
#define pdFREERTOS_ERRNO_EFAULT           14  /* Bad address */
#define pdFREERTOS_ERRNO_EBUSY            16  /* Mount device busy */
#define pdFREERTOS_ERRNO_EEXIST           17  /* File exists */
#define pdFREERTOS_ERRNO_EXDEV            18  /* Cross-device link */
#define pdFREERTOS_ERRNO_ENODEV           19  /* No such device */
#define pdFREERTOS_ERRNO_ENOTDIR          20  /* Not a directory */
#define pdFREERTOS_ERRNO_EISDIR           21  /* Is a directory */
#define pdFREERTOS_ERRNO_EINVAL           22  /* Invalid argument */
#define pdFREERTOS_ERRNO_ENOSPC           28  /* No space left on device */
#define pdFREERTOS_ERRNO_ESPIPE           29  /* Illegal seek */
#define pdFREERTOS_ERRNO_EROFS            30  /* Read only file system */
#define pdFREERTOS_ERRNO_EUNATCH          42  /* Protocol driver not attached */
#define pdFREERTOS_ERRNO_EBADE            50  /* Invalid exchange */
#define pdFREERTOS_ERRNO_EFTYPE           79  /* Inappropriate file type or format */
#define pdFREERTOS_ERRNO_ENMFILE          89  /* No more files */
#define pdFREERTOS_ERRNO_ENOTEMPTY        90  /* Directory not empty */
#define pdFREERTOS_ERRNO_ENAMETOOLONG     91  /* File or path name too long */
#define pdFREERTOS_ERRNO_EOPNOTSUPP       95  /* Operation not supported on transport endpoint */
#define pdFREERTOS_ERRNO_ENOBUFS          105 /* No buffer space available */
#define pdFREERTOS_ERRNO_ENOPROTOOPT      109 /* Protocol not available */
#define pdFREERTOS_ERRNO_EADDRINUSE       112 /* Address already in use */
#define pdFREERTOS_ERRNO_ETIMEDOUT        116 /* Connection timed out */
#define pdFREERTOS_ERRNO_EINPROGRESS      119 /* Connection already in progress */
#define pdFREERTOS_ERRNO_EALREADY         120 /* Socket already connected */
#define pdFREERTOS_ERRNO_EADDRNOTAVAIL    125 /* Address not available */
#define pdFREERTOS_ERRNO_EISCONN          127 /* Socket is already connected */
#define pdFREERTOS_ERRNO_ENOTCONN         128 /* Socket is not connected */
#define pdFREERTOS_ERRNO_ENOMEDIUM        135 /* No medium inserted */
#define pdFREERTOS_ERRNO_EILSEQ           138 /* An invalid UTF-16 sequence was encountered. */
#define pdFREERTOS_ERRNO_ECANCELED        140 /* Operation canceled. */

#define pdMS_TO_TICKS(x) MS_TO_TICKS((x))

static inline void vTaskDelay(const TickType_t xTicksToDelay)
{
	struct Timeout timeout = {0, xTicksToDelay};
	thread_sleep(&timeout);
}

static inline TickType_t xTaskGetTickCount(void)
{
	SystickReturn ret = thread_systemtick_get();

	return ret.lo;
}

static inline TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
	return thread_id_get();
}

static inline void* pvPortMalloc(size_t size)
{
	return malloc(size);
}

static inline void vPortFree(void* ptr)
{
	free(ptr);
}

static inline BaseType_t xQueueReceive(QueueHandle_t que, void* buf, TickType_t waitTicks)
{
	struct Timeout timeout = {0, waitTicks};
	int rv = queue_recv(&timeout, que, buf);

	if (rv == 0)
		return pdPASS;
	return errQUEUE_EMPTY;
}

static inline BaseType_t xQueueSendToBack(QueueHandle_t que, const void* buf, TickType_t waitTicks)
{
	struct Timeout timeout = {0, waitTicks};
	int rv = queue_send(&timeout, que, buf);

	if (rv == 0)
		return pdPASS;
	return errQUEUE_FULL;
}

static inline QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
	QueueHandle_t ret;
	struct Timeout timeout = {0, UnlimitedTimeout};
	int rc = queue_create(&timeout, MALLOC_CAPABILITY, &ret, uxItemSize, uxQueueLength);

	if (rc)
		return NULL;
	return ret;
}

static inline void vQueueDelete(QueueHandle_t xQueue)
{
	queue_delete(MALLOC_CAPABILITY, xQueue);
}

static inline UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue)
{
	size_t ret;
	int rv = queue_items_remaining(xQueue, &ret);

	assert(rv == 0);

	return ret;
}

static inline SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t uxMaxCount,
	UBaseType_t uxInitialCount)
{
	QueueHandle_t ret;
	int rv;

	assert(uxMaxCount == uxInitialCount);
	struct Timeout timeout = {0, UnlimitedTimeout};
	rv = semaphore_create(&timeout, MALLOC_CAPABILITY, &ret, uxMaxCount);

	if (rv)
		return NULL;
	return ret;
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t sema, TickType_t waitTicks)
{
	struct Timeout timeout = {0, waitTicks};
	int rv = semaphore_take(&timeout, sema);

	if (rv == 0)
		return pdTRUE;
	return pdFALSE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t sema)
{
	struct Timeout timeout = {0, UnlimitedTimeout};
	int rv = semaphore_give(&timeout, sema);

	if (rv != 0)
		return pdFALSE;
	return pdTRUE;
}

static inline void vTaskSetTimeOutState(TimeOut_t* timeout)
{
	SystickReturn ret = thread_systemtick_get();
	*timeout = ((uint64_t)ret.hi << 32) | ret.lo;
}

static inline BaseType_t xTaskCheckForTimeOut(TimeOut_t* pxTimeOut, TickType_t* pxTicksToWait)
{
    BaseType_t xReturn;
	uint64_t timeBlock;
	TickType_t xElapsedTime;

	vTaskSetTimeOutState(&timeBlock);

	xElapsedTime = timeBlock - *pxTimeOut;
	if( xElapsedTime < *pxTicksToWait ) {
		*pxTicksToWait -= xElapsedTime;
		*pxTimeOut = timeBlock;
		xReturn = pdFALSE;
	} else {
		*pxTicksToWait = ( TickType_t ) 0;
		xReturn = pdTRUE;
	}

    return xReturn;
}

static inline EventGroupHandle_t xEventGroupCreate(void)
{
	void* ret;
	struct Timeout timeout = {0, UnlimitedTimeout};
	int rv = event_create(&timeout, MALLOC_CAPABILITY, &ret);

	if (rv == 0)
		return ret;
	return NULL;
}

static inline EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup,
	const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit,
	const BaseType_t xWaitForAllBits, TickType_t xTicksToWait)
{
	uint32_t ret;
	struct Timeout timeout = {0, xTicksToWait};
	int rv = event_bits_wait(&timeout, xEventGroup, &ret, uxBitsToWaitFor, xClearOnExit, xWaitForAllBits);

	assert(rv == -ETIMEDOUT || rv == -EWOULDBLOCK || (rv == 0 && ret != 0));

	return ret;
}

static inline EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup,
	const EventBits_t uxBitsToClear)
{
	uint32_t ret;
	int rv = event_bits_clear(xEventGroup, &ret, uxBitsToClear);

	assert(rv == 0);

	return ret;
}

static inline EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup,
	const EventBits_t uxBitsToSet)
{
	uint32_t ret;
	int rv = event_bits_set(xEventGroup, &ret, uxBitsToSet);

	assert(rv == 0);

	return ret;
}

static inline void vEventGroupDelete(EventGroupHandle_t xEventGroup)
{
	int rv = event_delete(MALLOC_CAPABILITY, xEventGroup);

	assert(rv == 0);
}

extern SemaphoreHandle_t xCritialSection;
extern SemaphoreHandle_t xTaskSuspension;

static inline void
taskENTER_CRITICAL(void)
{
	BaseType_t rv = xSemaphoreTake(xCritialSection, UINT32_MAX);
	assert(rv == pdTRUE);
}

static inline void
taskEXIT_CRITICAL(void)
{
	BaseType_t rv = xSemaphoreGive(xCritialSection);
	assert(rv == pdTRUE);
}

static inline void
vTaskSuspendAll(void)
{
	BaseType_t rv = xSemaphoreTake(xTaskSuspension, UINT32_MAX);
	assert(rv == pdTRUE);
}

static inline void
vTaskResumeAll(void)
{
	BaseType_t rv = xSemaphoreGive(xTaskSuspension);
	assert(rv == pdTRUE);
}

#endif // _FREERTOS_COMPAT_H_

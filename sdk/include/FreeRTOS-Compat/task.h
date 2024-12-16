#pragma once
#include "FreeRTOS.h"
#include "thread.h"
#include <locks.h>
#define INC_TASK_H

/**
 * Initialise a timeout with the current number of elapsed ticks.  This is used
 * in a blocking function in conjunction with `xTaskCheckForTimeOut()`.  The
 * value returned here should be passed as the first argument of
 * `xTaskCheckForTimeOut()`.
 */
static inline void vTaskSetTimeOutState(TimeOut_t *timeout)
{
	SystickReturn ret = thread_systemtick_get();
	*timeout          = ((uint64_t)ret.hi << 32) | ret.lo;
}

/**
 * Return whether a timeout has expired.  The first argument should be a
 * pointer to the value returned by `vTaskSetTimeOutState()` before any
 * blocking operations.  The second argument is the number of ticks to wait.
 * This function returns `pdTRUE` if the timeout has expired, or `pdFALSE` if
 * the timeout has not expired, and updates the arguments to reflect the
 * remaining time to wait.
 */
static inline BaseType_t xTaskCheckForTimeOut(TimeOut_t  *pxTimeOut,
                                              TickType_t *pxTicksToWait)
{
	BaseType_t xReturn;
	uint64_t   timeBlock;
	TickType_t xElapsedTime;

	vTaskSetTimeOutState(&timeBlock);

	xElapsedTime = timeBlock - *pxTimeOut;
	if (xElapsedTime < *pxTicksToWait)
	{
		*pxTicksToWait -= xElapsedTime;
		*pxTimeOut = timeBlock;
		xReturn    = pdFALSE;
	}
	else
	{
		*pxTicksToWait = (TickType_t)0;
		xReturn        = pdTRUE;
	}

	return xReturn;
}

/**
 * Block the current thread for a given number of ticks.
 */
static inline void vTaskDelay(const TickType_t xTicksToDelay)
{
	struct Timeout timeout = {0, xTicksToDelay};
	/*
	 * The FreeRTOS API does not have a way to signal failure of sleep, so we
	 * override the nodiscard annotation on thread_sleep.
	 */
	(void)thread_sleep(&timeout, ThreadSleepNoEarlyWake);
}

/**
 * Return the number of ticks elapsed since the system booted.  This is
 * truncated to a 32-bit value.
 */
static inline TickType_t xTaskGetTickCount(void)
{
	return thread_systemtick_get().lo;
}

/**
 * Returns the current thread ID.
 */
static inline TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
	return thread_id_get();
}

__BEGIN_DECLS

/**
 * Lock used to simulate disabling interrupts in `taskENTER_CRITICAL` and
 * `taskEXIT_CRITICAL`.  Code using these APIs must provide a definition to
 * accompany this declaration.
 */
extern struct RecursiveMutexState __CriticalSectionFlagLock;

/**
 * Lock used to simulate disabling interrupts in `vTaskSuspendAll` and
 * `xTaskResumeAll`.  Code using these APIs must provide a definition to
 * accompany this declaration.
 */
extern struct RecursiveMutexState __SuspendFlagLock;

__END_DECLS

/**
 * Critical section.  This acquires a recursive mutex.  In FreeRTOS, this
 * disables interrupts.  The CHERIoT RTOS security model does not permit
 * interrupts to be arbitrarily disabled, only for designated scopes.  Callers
 * of this should be carefully audited to ensure that they are safe.
 */
static inline void taskENTER_CRITICAL(void)
{
	Timeout t = {0, UnlimitedTimeout};
	recursivemutex_trylock(&t, &__CriticalSectionFlagLock);
}

/**
 * Exit a critical section entered with `taskENTER_CRITICAL`.
 */
static inline void taskEXIT_CRITICAL(void)
{
	recursivemutex_unlock(&__CriticalSectionFlagLock);
}

/**
 * Critical section.  This acquires a recursive mutex.  In FreeRTOS, this
 * disables interrupts.  The CHERIoT RTOS security model does not permit
 * interrupts to be arbitrarily disabled, only for designated scopes.  Callers
 * of this should be carefully audited to ensure that they are safe.
 */
static inline void vTaskSuspendAll(void)
{
	Timeout t = {0, UnlimitedTimeout};
	recursivemutex_trylock(&t, &__SuspendFlagLock);
}

/**
 * Exit a critical section entered with `vTaskSuspendAll`.
 */
static inline BaseType_t xTaskResumeAll(void)
{
	recursivemutex_unlock(&__SuspendFlagLock);
	return pdTRUE;
}

/**
 * Exit a critical section entered with `vTaskSuspendAll`.
 */
static inline void vTaskResumeAll(void)
{
	recursivemutex_unlock(&__SuspendFlagLock);
}

/**
 * Task creation API.  CHERIoT RTOS does not permit dynamic thread creation and
 * so this simply provides a warning so that ported code can be modified to
 * avoid the API.
 */
#define xTaskCreate(...)                                                       \
	_Pragma("GCC warning \"Dynamic thread creation is not supported\"")(       \
	  TaskHandle_t) -                                                          \
	  1

/**
 * Task creation API.  CHERIoT RTOS does not permit dynamic thread creation and
 * so this simply provides a warning so that ported code can be modified to
 * avoid the API.
 */
#define xTaskCreateStatic(...)                                                 \
	_Pragma("GCC warning \"Dynamic thread creation is not supported\"")(       \
	  TaskHandle_t) -                                                          \
	  1

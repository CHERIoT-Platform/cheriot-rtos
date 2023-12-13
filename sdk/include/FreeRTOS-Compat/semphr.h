#pragma once
#include "FreeRTOS.h"
#include <locks.h>
#include <stdatomic.h>

/**
 * Type for a handle to a counting semaphore.
 */
typedef struct CountingSemaphoreState *SemaphoreHandle_t;

/**
 * Yupe used to define space for storing a semaphore.
 */
typedef struct CountingSemaphoreState StaticSemaphore_t;

/**
 * Initialise a statically allocated semaphore.  The initial value and maximum
 * are specified with `uxInitialCount` and `uxMaxCount` respectively.
 *
 * Returns a pointer to the semaphore.
 */
__always_inline static inline SemaphoreHandle_t
xSemaphoreCreateCountingStatic(UBaseType_t        uxMaxCount,
                               UBaseType_t        uxInitialCount,
                               StaticSemaphore_t *pxSemaphoreBuffer)
{
	pxSemaphoreBuffer->maxCount = uxMaxCount;
#ifdef __cplusplus
	pxSemaphoreBuffer->count = uxInitialCount;
#else
	// In C mode, we don't want a variable-sized atomic store here!
	atomic_init(&pxSemaphoreBuffer->count, uxInitialCount);
#endif
	return pxSemaphoreBuffer;
}

/**
 * Allocate a counting semaphore on the heap.  The initial value and maximum are
 * specified with `uxInitialCount` and `uxMaxCount` respectively.
 *
 * Returns a pointer to the semaphore on success, NULL on allocation failure.
 */
__always_inline static inline SemaphoreHandle_t
xSemaphoreCreateCounting(UBaseType_t uxMaxCount, UBaseType_t uxInitialCount)
{
	SemaphoreHandle_t semaphore =
	  (SemaphoreHandle_t)malloc(sizeof(struct CountingSemaphoreState));
	if (semaphore != NULL)
	{
		xSemaphoreCreateCountingStatic(uxMaxCount, uxInitialCount, semaphore);
	}
	return semaphore;
}

/**
 * Delete a heap-allocated semaphore.
 */
__always_inline static inline void
vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
	free(xSemaphore);
}

/**
 * A semaphore get (down) operation.  If the semaphore value is zero, this can
 * block for up to `xTicksToWait` ticks.  Returns true if the semaphore was
 * obtained, false if the timeout expired.
 */
__always_inline static inline _Bool xSemaphoreTake(SemaphoreHandle_t xSemaphore,
                                                   TickType_t xTicksToWait)
{
	Timeout t   = {0, xTicksToWait};
	int     ret = semaphore_get(&t, xSemaphore);
	return ret == 0;
}

/**
 * A semaphore put (up) operation.  If there are tasks blocked on the semaphore,
 * this will unblock one of them.  Otherwise, the semaphore value is incremented
 * by one.
 */
__always_inline static inline _Bool xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
	int ret = semaphore_put(xSemaphore);
	return ret == 0;
}

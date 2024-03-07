#pragma once
/**
 * FreeRTOS semaphore compatibility layer.  This maps FreeRTOS semaphore types
 * to their CHERIoT RTOS equivalents.
 *
 * There is some overhead from dynamic dispatch that can be avoided if only one
 * of the FreeRTOS semaphore types needs to be supported in a particular
 * component.  You can enable individual semaphore types by defining the
 * following macros:
 *
 * - `CHERIOT_EXPOSE_FREERTOS_SEMAPHORE`: Enable counting and binary semaphores.
 * - `CHERIOT_EXPOSE_FREERTOS_MUTEX`: Enable non-recursive, priority-inheriting
 * mutexes.
 * - `CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX`: Enable recursive mutexes.
 */
#include "FreeRTOS.h"
#include <locks.h>
#include <stdatomic.h>

#if !defined(CHERIOT_EXPOSE_FREERTOS_SEMAPHORE) &&                             \
  !defined(CHERIOT_EXPOSE_FREERTOS_MUTEX) &&                                   \
  !defined(CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX)
#	error At least one out of CHERIOT_EXPOSE_FREERTOS_SEMAPHORE, \
  CHERIOT_EXPOSE_FREERTOS_MUTEX, or CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX \
  must be defined.
#endif

#ifdef CHERIOT_EXPOSE_FREERTOS_SEMAPHORE
#	define CHERIOT_FREERTOS_SEMAPHORE_CASE(x)                                 \
		case CheriotFreeRTOS_Semaphore:                                        \
			x;                                                                 \
			break;
#else
#	define CHERIOT_FREERTOS_SEMAPHORE_CASE(x)
#endif

#ifdef CHERIOT_EXPOSE_FREERTOS_MUTEX
#	define CHERIOT_FREERTOS_MUTEX_CASE(x)                                     \
		case CheriotFreeRTOS_Mutex:                                            \
			x;                                                                 \
			break;
#else
#	define CHERIOT_FREERTOS_MUTEX_CASE(x)
#endif

#ifdef CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX
#	define CHERIOT_FREERTOS_RECURSIVE_MUTEX_CASE(x)                           \
		case CheriotFreeRTOS_RecursiveMutex:                                   \
			x;                                                                 \
			break;
#else
#	define CHERIOT_FREERTOS_RECURSIVE_MUTEX_CASE(x)
#endif

/**
 * Enumeration used to differentiate the different kinds of FreeRTOS semaphores
 * in this compatibility layer.
 */
enum CheriotFreeRTOS_SemaphoreKind : uint8_t
{
#ifdef CHERIOT_EXPOSE_FREERTOS_SEMAPHORE
	/**
	 * A FreeRTOS counting or binary semaphore.
	 */
	CheriotFreeRTOS_Semaphore,
#endif
#ifdef CHERIOT_EXPOSE_FREERTOS_MUTEX
	/**
	 * A non-recursive, priority-inheriting mutex.
	 */
	CheriotFreeRTOS_Mutex,
#endif
#ifdef CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX
	/**
	 * A recursive mutex.
	 */
	CheriotFreeRTOS_RecursiveMutex,
#endif
};

/**
 * Type used to define space for storing any of the exposed FreeRTOS semaphore
 * types.
 */
typedef struct
{
	/**
	 * The state for the various kinds of FreeRTOS semaphores.
	 */
	union
	{
#ifdef CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX
		struct RecursiveMutexState recursiveMutex;
#endif
#ifdef CHERIOT_EXPOSE_FREERTOS_MUTEX
		struct FlagLockState mutex;
#endif
#ifdef CHERIOT_EXPOSE_FREERTOS_SEMAPHORE
		struct CountingSemaphoreState semaphore;
#endif
	};
#if (defined(CHERIOT_EXPOSE_FREERTOS_SEMAPHORE) +                              \
     defined(CHERIOT_EXPOSE_FREERTOS_MUTEX) +                                  \
     defined(CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX)) > 1
#	define CHERIOT_FREERTOS_SEMAPHORE_SWITCH(x) switch (x->kind)
	/**
	 * The discriminator for the union above.  If only one kind of semaphore is
	 * supported, this is omitted.
	 */
	enum CheriotFreeRTOS_SemaphoreKind kind;
#	define CHERIOT_FREERTOS_SEMAPHORE_KIND_SET(x, y) x->kind = y
#else
#	if defined(CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX)
#		define CHERIOT_FREERTOS_SEMAPHORE_SWITCH(x)                           \
			switch (CheriotFreeRTOS_RecursiveMutex)
#	elif defined(CHERIOT_EXPOSE_FREERTOS_MUTEX)
#		define CHERIOT_FREERTOS_SEMAPHORE_SWITCH(x)                           \
			switch (CheriotFreeRTOS_Mutex)
#	elif defined(CHERIOT_EXPOSE_FREERTOS_SEMAPHORE)
#		define CHERIOT_FREERTOS_SEMAPHORE_SWITCH(x)                           \
			switch (CheriotFreeRTOS_Semaphore)
#	endif

#	define CHERIOT_FREERTOS_SEMAPHORE_KIND_SET(x, y)
#endif
} StaticSemaphore_t;

/**
 * Type for a handle to a counting semaphore.
 */
typedef StaticSemaphore_t *SemaphoreHandle_t;

#ifdef CHERIOT_EXPOSE_FREERTOS_SEMAPHORE
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
	pxSemaphoreBuffer->semaphore.maxCount = uxMaxCount;
#	ifdef __cplusplus
	pxSemaphoreBuffer->semaphore.count = uxInitialCount;
#	else
	// In C mode, we don't want a variable-sized atomic store here!
	atomic_init(&pxSemaphoreBuffer->semaphore.count, uxInitialCount);
#	endif
	CHERIOT_FREERTOS_SEMAPHORE_KIND_SET(pxSemaphoreBuffer,
	                                    CheriotFreeRTOS_Semaphore);
	return pxSemaphoreBuffer;
}

#	ifndef CHERIOT_NO_AMBIENT_MALLOC
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
	  (SemaphoreHandle_t)malloc(sizeof(StaticSemaphore_t));
	if (semaphore != NULL)
	{
		xSemaphoreCreateCountingStatic(uxMaxCount, uxInitialCount, semaphore);
	}
	return semaphore;
}
#	endif

/**
 * Create a heap-allocated binary semaphore.
 *
 * Binary semaphores are implemented as counting semaphores with a maximum count
 * of 1.
 */
__always_inline static inline SemaphoreHandle_t xSemaphoreCreateBinary()
{
	return xSemaphoreCreateCounting(1, 0);
}

/**
 * Create a statically allocated binary semaphore.
 *
 * Binary semaphores are implemented as counting semaphores with a maximum count
 * of 1.
 */
__always_inline static inline SemaphoreHandle_t
xSemaphoreCreateBinaryStatic(StaticSemaphore_t *pxSemaphoreBuffer)
{
	return xSemaphoreCreateCountingStatic(1, 0, pxSemaphoreBuffer);
}
#endif

#ifdef CHERIOT_EXPOSE_FREERTOS_MUTEX
/**
 * Create statically allocated mutex.
 */
__always_inline static inline SemaphoreHandle_t
xSemaphoreCreateMutexStatic(StaticSemaphore_t *pxMutexBuffer)
{
	pxMutexBuffer->mutex.lockWord = 0;
	CHERIOT_FREERTOS_SEMAPHORE_KIND_SET(pxMutexBuffer, CheriotFreeRTOS_Mutex);
	return pxMutexBuffer;
}

#	ifndef CHERIOT_NO_AMBIENT_MALLOC
/**
 * Create a heap-allocated mutex.
 */
__always_inline static inline SemaphoreHandle_t xSemaphoreCreateMutex()
{
	SemaphoreHandle_t semaphore =
	  (SemaphoreHandle_t)malloc(sizeof(StaticSemaphore_t));
	if (semaphore != NULL)
	{
		xSemaphoreCreateMutexStatic(semaphore);
	}
	return semaphore;
}
#	endif
#endif

#ifdef CHERIOT_EXPOSE_FREERTOS_RECURSIVE_MUTEX

/**
 * Create a statically allocated recursive mutex.
 */
__always_inline static inline SemaphoreHandle_t
xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *pxMutexBuffer)
{
	pxMutexBuffer->recursiveMutex.lock.lockWord = 0;
	pxMutexBuffer->recursiveMutex.depth         = 0;
	CHERIOT_FREERTOS_SEMAPHORE_KIND_SET(pxMutexBuffer,
	                                    CheriotFreeRTOS_RecursiveMutex);
	return pxMutexBuffer;
}

#	ifndef CHERIOT_NO_AMBIENT_MALLOC
/**
 * Create a heap-allocated recursive mutex.
 */
__always_inline static inline SemaphoreHandle_t
xSemaphoreCreateRecursiveMutex(void)
{
	SemaphoreHandle_t semaphore =
	  (SemaphoreHandle_t)malloc(sizeof(StaticSemaphore_t));
	if (semaphore != NULL)
	{
		xSemaphoreCreateRecursiveMutexStatic(semaphore);
	}
	return semaphore;
}
#	endif
#endif

/**
 * Delete a heap-allocated semaphore, of any kind.
 *
 * Note: As on FreeRTOS, if there are waiters blocked on this semaphore then
 * they will remain blocked until their timeout (if ever).
 */
__always_inline static inline void
vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
	free(xSemaphore);
}

/**
 * Get the current owner (thread ID) of a mutex.  If this is a counting
 * semaphore, or if the lock is not held, returns zero.
 *
 * Note: This API is inherently somewhat racy.  If this returns the current
 * thread's thread ID, then it cannot change, but any other return value may
 * change between the time it is returned and the time the caller inspects it.
 */
__always_inline static inline TaskHandle_t
xSemaphoreGetMutexHolder(SemaphoreHandle_t xMutex)
{
	CHERIOT_FREERTOS_SEMAPHORE_SWITCH(xMutex)
	{
		CHERIOT_FREERTOS_SEMAPHORE_CASE(return 0;)
		CHERIOT_FREERTOS_RECURSIVE_MUTEX_CASE(
		  return xMutex->recursiveMutex.lock.lockWord & 0xffff;)
		CHERIOT_FREERTOS_MUTEX_CASE(return xMutex->mutex.lockWord & 0xffff;)
	}
	return 0;
}

/**
 * Returns the current count of a counting semaphore, or whether a mutex or
 * binary semaphore is available
 *
 * Note: This API is inherently racy.  Unless run with interrupts disabled,
 * there is no guarantee that the value returned is still valid by the time the
 * caller inspects it.
 */
static inline UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t xSemaphore)
{
	CHERIOT_FREERTOS_SEMAPHORE_SWITCH(xSemaphore)
	{
		CHERIOT_FREERTOS_SEMAPHORE_CASE(return xSemaphore->semaphore.count;)
		CHERIOT_FREERTOS_RECURSIVE_MUTEX_CASE(
		  return xSemaphore->recursiveMutex.lock.lockWord != 0;)
		CHERIOT_FREERTOS_MUTEX_CASE(return xSemaphore->mutex.lockWord != 0;)
	}
	return 0;
}

/**
 * A semaphore get (down) operation.  If the semaphore value is zero, this can
 * block for up to `xTicksToWait` ticks.  Returns true if the semaphore was
 * obtained, false if the timeout expired.
 */
static inline _Bool xSemaphoreTake(SemaphoreHandle_t xSemaphore,
                                   TickType_t        xTicksToWait)
{
	Timeout t   = {0, xTicksToWait};
	int     ret = -1;
	CHERIOT_FREERTOS_SEMAPHORE_SWITCH(xSemaphore)
	{
		CHERIOT_FREERTOS_SEMAPHORE_CASE(
		  ret = semaphore_get(&t, &xSemaphore->semaphore);)
		CHERIOT_FREERTOS_MUTEX_CASE(
		  ret = flaglock_priority_inheriting_trylock(&t, &xSemaphore->mutex);)
		// Recursive mutexes are not supposed to use this interface.
		CHERIOT_FREERTOS_RECURSIVE_MUTEX_CASE(;);
	}
	return ret == 0;
}

/**
 * Take a semaphore from code that may run in an ISR.  CHERIoT RTOS does not
 * permit code to run in ISRs and so this simply calls the normal code paths.
 *
 * The second parameter is a pointer to a variable that will be set to true if
 * the caller needs to yield to wake a higher-priority task.  This is
 * unconditionally false on CHERIoT RTOS because the scheduler will wake any
 * threads that are waiting on a semaphore.  It is necessary on FreeRTOS
 * because code in ISRs cannot call into the scheduler.
 */
__always_inline static inline _Bool __attribute__((overloadable))
xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore,
                      BaseType_t       *pxHigherPriorityTaskWoken)
{
	*pxHigherPriorityTaskWoken = 0;
	return xSemaphoreTake(xSemaphore, 0);
}

/*
 * Take a semaphore from code that may run in an ISR.  CHERIoT RTOS does not
 * permit code to run in ISRs and so this simply calls the normal code paths.
 */
__always_inline static inline _Bool __attribute__((overloadable))
xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore)
{
	return xSemaphoreTake(xSemaphore, 0);
}

/**
 * A semaphore put (up) operation.  If there are tasks blocked on the semaphore,
 * this will unblock one of them.  Otherwise, the semaphore value is incremented
 * by one.
 */
__always_inline static inline _Bool xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
	int ret = -1;
	CHERIOT_FREERTOS_SEMAPHORE_SWITCH(xSemaphore)
	{
		CHERIOT_FREERTOS_SEMAPHORE_CASE(
		  ret = semaphore_put(&xSemaphore->semaphore);)
		CHERIOT_FREERTOS_MUTEX_CASE(flaglock_unlock(&xSemaphore->mutex);
		                            ret = 0;)
		// Recursive mutexes are not supposed to use this interface.
		CHERIOT_FREERTOS_RECURSIVE_MUTEX_CASE(;);
	}
	return ret == 0;
}

/**
 * Give a semaphore from code that may run in an ISR.  CHERIoT RTOS does not
 * permit code to run in ISRs and so this simply calls the normal code paths.
 *
 * The second parameter is a pointer to a variable that will be set to true if
 * the caller needs to yield to wake a higher-priority task.  This is
 * unconditionally false on CHERIoT RTOS.
 */
__always_inline static inline _Bool __attribute__((overloadable))
xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore,
                      BaseType_t       *pxHigherPriorityTaskWoken)
{
	*pxHigherPriorityTaskWoken = 0;
	return xSemaphoreGive(xSemaphore);
}

/*
 * Give a semaphore from code that may run in an ISR.  CHERIoT RTOS does not
 * permit code to run in ISRs and so this simply calls the normal code paths.
 */
__always_inline static inline _Bool __attribute__((overloadable))
xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore)
{
	return xSemaphoreGive(xSemaphore);
}

/**
 * Try to acquire a recursive mutex.  Returns true on success, false on
 * failure.
 */
__always_inline static inline _Bool
xSemaphoreTakeRecursive(SemaphoreHandle_t xMutex, TickType_t xTicksToWait)
{
	Timeout t   = {0, xTicksToWait};
	int     ret = -1;
	CHERIOT_FREERTOS_SEMAPHORE_SWITCH(xMutex)
	{
		CHERIOT_FREERTOS_SEMAPHORE_CASE(;)
		CHERIOT_FREERTOS_MUTEX_CASE(;)
		CHERIOT_FREERTOS_RECURSIVE_MUTEX_CASE(
		  ret = recursivemutex_trylock(&t, &xMutex->recursiveMutex);)
	}
	return ret == 0;
}

/**
 * Release a recursive mutex.
 */
__always_inline static inline _Bool
xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex)
{
	int ret = -1;
	CHERIOT_FREERTOS_SEMAPHORE_SWITCH(xMutex)
	{
		CHERIOT_FREERTOS_SEMAPHORE_CASE(;)
		CHERIOT_FREERTOS_MUTEX_CASE(;)
		CHERIOT_FREERTOS_RECURSIVE_MUTEX_CASE(
		  ret = recursivemutex_unlock(&xMutex->recursiveMutex);)
	}
	return ret == 0;
}

/**
 * Launder a semaphore back into the type used to initialise it.
 */
__always_inline static inline BaseType_t
xSemaphoreGetStaticBuffer(SemaphoreHandle_t   xSemaphore,
                          StaticSemaphore_t **ppxSemaphoreBuffer)
{
	*ppxSemaphoreBuffer = xSemaphore;
	return pdTRUE;
}

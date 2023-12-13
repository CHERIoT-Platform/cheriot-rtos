#pragma once
#include "cdefs.h"
#include <assert.h>
#include <stdint.h>
#include <tick_macros.h>
#include <timeout.h>

#define pdMS_TO_TICKS(x) MS_TO_TICKS((x))

/**
 * Guard macro.  Checked in other headers, CHERIoT RTOS prefers `#pragma once`
 * for include guards and avoids header dependency orders.
 */
#define INC_FREERTOS_H

#define PRIVILEGED_FUNCTION

#define pdFREERTOS_LITTLE_ENDIAN 0
#define pdFREERTOS_BIG_ENDIAN 1

#define pdFALSE ((BaseType_t)0)
#define pdTRUE ((BaseType_t)1)

#define pdFAIL pdFALSE
#define pdPASS pdTRUE

#ifdef NDEBUG
#	define configASSERT(x)                                                    \
		do                                                                     \
		{                                                                      \
		} while (0)
#else
#	define configASSERT(x)                                                    \
		((x) ? ((void)0)                                                       \
		     : (printf("Assertion failure in %s() %s:%d: %s\n",                \
		               __func__,                                               \
		               __FILE_NAME__,                                          \
		               __LINE__,                                               \
		               #x),                                                    \
		        panic(),                                                       \
		        (void)0))
#endif

#define configTICK_RATE_HZ TICK_RATE_HZ
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS MS_PER_TICK

#ifndef traceENTER_vListInitialise
#	define traceENTER_vListInitialise(...)                                    \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceRETURN_vListInitialise
#	define traceRETURN_vListInitialise(...)                                   \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceENTER_vListInitialiseItem
#	define traceENTER_vListInitialiseItem(...)                                \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceRETURN_vListInitialiseItem
#	define traceRETURN_vListInitialiseItem(...)                               \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceENTER_vListInsertEnd
#	define traceENTER_vListInsertEnd(...)                                     \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceRETURN_vListInsertEnd
#	define traceRETURN_vListInsertEnd(...)                                    \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef mtCOVERAGE_TEST_DELAY
#	define mtCOVERAGE_TEST_DELAY()                                            \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef mtCOVERAGE_TEST_MARKER
#	define mtCOVERAGE_TEST_MARKER()                                           \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceENTER_vListInsert
#	define traceENTER_vListInsert(...)                                        \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceRETURN_vListInsert
#	define traceRETURN_vListInsert(...)                                       \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceENTER_uxListRemove
#	define traceENTER_uxListRemove(...)                                       \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

#ifndef traceRETURN_uxListRemove
#	define traceRETURN_uxListRemove(...)                                      \
		do                                                                     \
		{                                                                      \
		} while (0)
#endif

/**
 * FreeRTOS type for durations expressed in ticks.
 */
typedef Ticks TickType_t;
/**
 * FreeRTOS default signed integer type.
 */
typedef int32_t BaseType_t;
/**
 * FreeRTOS default unsigned integer type.
 */
typedef uint32_t UBaseType_t;
/**
 * FreeRTOS type representing a task handle, mapped to a thread ID in CHERIoT
 * RTOS.
 */
typedef uint16_t TaskHandle_t;
/**
 * FreeRTOS type for a duration expressed in large numbers of ticks.
 */
typedef uint64_t TimeOut_t;

// Some things include this file expecting to get other FreeRTOS headers,
// others include those files directly.
#include "event_groups.h"
#include "task.h"
#include "queue.h"

// Some things expect this to include list.h.  This is an incredibly complex
// data structure that it is not worth reimplementing, so include it if it
// exists and allow components to copy it from FreeRTOS if they want it (it's
// MIT licensed).
#if __has_include(<list.h>)
#include <list.h>
#endif

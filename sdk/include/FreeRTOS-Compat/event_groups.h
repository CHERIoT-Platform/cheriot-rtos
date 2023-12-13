#pragma once
#include "FreeRTOS.h"
#include <event.h>
#include <stdint.h>

/**
 * Type for bits in an event group.
 */
typedef uint32_t EventBits_t;

/**
 * Type for event group handles.
 */
typedef struct EventGroup *EventGroupHandle_t;

/**
 * Create a new event group on the heap.  Returns NULL on allocation failure.
 */
static inline EventGroupHandle_t xEventGroupCreate(void)
{
	struct EventGroup *ret;
	struct Timeout     timeout = {0, UnlimitedTimeout};
	int rv = eventgroup_create(&timeout, MALLOC_CAPABILITY, &ret);

	if (rv == 0)
	{
		return ret;
	}
	return NULL;
}

/**
 * Set bits in an event group.  Returns the new value of the event group after
 * this call.  If the timeout expires, the new bits may not have been set.
 */
static inline EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup,
                                             const EventBits_t  uxBitsToSet)
{
	EventBits_t    ret;
	struct Timeout timeout = {0, UnlimitedTimeout};
	eventgroup_set(&timeout, xEventGroup, &ret, uxBitsToSet);
	return ret;
}

/**
 * Wait for bits in an event group to be set.  Returns the new value of the
 * bits that are currently set.  Blocks until either the timeout expires or
 * some or all `uxBitsToWaitFor` are set, depending on the value of
 * `xWaitForAllBits`. If `xClearOnExit` is true, the bits that were set are
 * cleared before returning.
 */
static inline EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup,
                                              const EventBits_t uxBitsToWaitFor,
                                              const BaseType_t  xClearOnExit,
                                              const BaseType_t  xWaitForAllBits,
                                              TickType_t        xTicksToWait)
{
	uint32_t       ret;
	struct Timeout timeout = {0, xTicksToWait};
	int            rv      = eventgroup_wait(&timeout,
	                                         xEventGroup,
	                                         &ret,
	                                         uxBitsToWaitFor,
	                                         xClearOnExit,
	                                         xWaitForAllBits);
	return ret;
}

/**
 * Delete an event group.
 */
static inline void vEventGroupDelete(EventGroupHandle_t xEventGroup)
{
	free(xEventGroup);
}

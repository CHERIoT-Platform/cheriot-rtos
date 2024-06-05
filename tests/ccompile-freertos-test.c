#include <FreeRTOS-Compat/FreeRTOS.h>

#if (CHERIOT_FREERTOS_SEMAPHORE + CHERIOT_FREERTOS_MUTEX +                     \
      CHERIOT_FREERTOS_RECURSIVE_MUTEX) == 1
#if CHERIOT_FREERTOS_SEMAPHORE == 1
_Static_assert(sizeof(StaticSemaphore_t) == sizeof(struct CountingSemaphoreState);
#elif CHERIOT_FREERTOS_MUTEX == 1
_Static_assert(sizeof(StaticSemaphore_t) == sizeof(struct FlagLockState));
#else
_Static_assert(sizeof(StaticSemaphore_t) == sizeof(struct RecursiveMutexState));
#endif
#endif

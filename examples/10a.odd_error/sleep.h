// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <thread.h>
#include <tick_macros.h>

static inline void sleep(const uint32_t mS)
{
	Timeout t1{MS_TO_TICKS(mS)};
	thread_sleep(&t1, ThreadSleepNoEarlyWake);
}
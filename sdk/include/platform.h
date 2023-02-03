// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <stdint.h>

#ifndef CPU_TIMER_HZ
#	error "Timer frequency CPU_TIMER_HZ must be defined."
#endif

#ifndef TICK_RATE_HZ
#	error "Scheduler tick rate TICK_RATE_HZ must be defined."
#endif
#define TIMERCYCLES_PER_TICK (CPU_TIMER_HZ / TICK_RATE_HZ)
#define MS_PER_TICK (1000U / TICK_RATE_HZ)

#define MS_TO_TICKS(x) ((x) / MS_PER_TICK)

typedef struct
{
	// 16550A registers
	uint32_t data;
	uint32_t intrEnable;
	uint32_t intrIDandFifo;
	uint32_t lineControl;
	uint32_t modemControl;
	struct __packed
	{
		uint32_t rxReady : 1;
		uint32_t overrun : 1;
		uint32_t parityError : 1;
		uint32_t framingError : 1;
		uint32_t breakIndicator : 1;
		uint32_t txBufEmpty : 1;
		uint32_t unused : 26;
	} lineStatus;
	uint32_t modemStatus;
	uint32_t scratch;
} Uart;

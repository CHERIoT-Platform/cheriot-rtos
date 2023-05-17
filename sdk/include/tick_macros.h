// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#ifndef CPU_TIMER_HZ
#	error "Timer frequency CPU_TIMER_HZ must be defined."
#endif

#ifndef TICK_RATE_HZ
#	error "Scheduler tick rate TICK_RATE_HZ must be defined."
#endif
#define TIMERCYCLES_PER_TICK (CPU_TIMER_HZ / TICK_RATE_HZ)
#define MS_PER_TICK (1000U / TICK_RATE_HZ)

#define MS_TO_TICKS(x) ((x) / MS_PER_TICK)


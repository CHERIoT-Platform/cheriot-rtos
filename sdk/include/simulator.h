// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <compartment.h>
#include <errno.h>
#include <stdint.h>

#ifdef SIMULATION
/**
 * Exit simulation, reporting the error code given as the argument.
 */
[[cheri::interrupt_state(disabled)]] int __cheri_compartment("sched")
  simulation_exit(uint32_t code = 0);
#else
static inline int simulation_exit(uint32_t code)
{
	return -ENOTSUP;
};
#endif

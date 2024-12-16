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
  scheduler_simulation_exit(uint32_t code __if_cxx(= 0));
#endif

/**
 * Exit the simulation, if we can, or fall back to an infinite loop.
 */
static inline void __attribute__((noreturn))
simulation_exit(uint32_t code __if_cxx(= 0))
{
#ifdef SIMULATION
	/*
	 * This fails only if either we are out of (trusted) stack space for the
	 * cross-call or the platform is misconfigured.  If either of those happen,
	 * fall back to infinite looping.
	 */
	(void)scheduler_simulation_exit(code);
#endif

	while (true)
	{
		yield();
	}
};

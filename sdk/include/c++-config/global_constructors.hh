// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cheri.hh>
#include <riscvreg.h>
#include <stddef.h>

/**
 * Support for running global constructors.  There is no global initialisation
 * phase for all compartments, so this provides support for lazily running
 * global constructors on demand.
 */
namespace GlobalConstructors
{
	/**
	 * Helper that runs all global constructors in its constructor.
	 */
	struct ConstructHelper
	{
		/**
		 * Iterate over the global constructors array, calling each
		 * constructor.
		 */
		ConstructHelper()
		{
			using GlobalConstructor = void (*)();
			ptraddr_t initStart     = LA_ABS(__init_array_start);
			ptraddr_t initEnd       = LA_ABS(__init_array_end);
			CHERI::Capability<void (*)()> pcc{
			  static_cast<void (**)()>(__builtin_cheri_program_counter_get())};
			for (ptraddr_t i = initStart; i < initEnd;
			     i += sizeof(GlobalConstructor))
			{
				pcc.address() = i;
				(*pcc)();
			}
		}
	};

	/**
	 * Call all global constructors in this compartment, precisely once.
	 */
	void run()
	{
		// This will emit a guard variable and code that checks the guard
		// variable and calls the constructor if it has not been called.
		static ConstructHelper helper;
	}
}; // namespace GlobalConstructors

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <debug.hh>
#include <thread.h>

#include "data.h"

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Config Data">;

//
// Helper function for the example to print a config item
//
void __cheri_libcall print_config(const char *name, Data *d)
{
	if (d == nullptr)
	{
		Debug::log("thread {} {} -- No config yet", thread_id_get(), name);
	}
	else
	{
		Debug::log("thread {} {} -- config count: {} token: {}",
		           thread_id_get(),
		           name,
		           d->count,
		           static_cast<const char *>(d->token));
	}
}

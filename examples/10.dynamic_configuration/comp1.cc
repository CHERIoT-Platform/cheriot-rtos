// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <compartment.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <thread.h>

// Define a sealed capability that gives this compartment
// read access to configuration data "config1"
#include "config_broker.h"
DEFINE_CONFIG_CAPABILITY("config1")

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Compartment #1">;

#include "data.h"
#include "sleep.h"
#include "validator.h"

// Local pointer to the most recent config value;
Data *config = nullptr;

//
// Callback to handle config updates
//
void __cheri_callback update(const char *id, void *data)
{
	// Validate the data in a sandpit compartment
	bool valid = false;
	validate(data, &valid);
	if (!valid)
	{
		Debug::log(
		  "thread {} Validation failed for {} {}", thread_id_get(), id, data);
		return;
	}

	// If we've been given a different object from
	// last time then release our claim on the old
	// object and add a claim to the new object so
	// we still have access to it outside the scope
	// of this call
	if ((config != data))
	{
		if (config != nullptr)
		{
			heap_free(MALLOC_CAPABILITY, config);
		}
		heap_claim(MALLOC_CAPABILITY, data);
	}

	config = static_cast<Data *>(data);
	print_config("Update", id, config);
}

//
// Thread entry point.
//
void __cheri_compartment("comp1") init()
{
	// Register to get config updates
	Debug::log("thread {} Register for config updates", thread_id_get());
	on_config(CONFIG_CAPABILITY, update);

	// Loop printing our config value occasionally
	while (true)
	{
		sleep(4500);
		print_config("Timer ", "config1", config);

		// Check we're not leaking data;
		// Debug::log("heap quota available: {}",
		//   heap_quota_remaining(MALLOC_CAPABILITY));
	}
}

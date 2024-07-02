// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <thread.h>

// Define a sealed capability that gives this compartment
// read access to configuration data "config1"
#include "config_broker.h"
#include "futex.h"

// Config Item this subscriber is interested in
#define CONFIG_ITEM_NAME "config1"
DEFINE_READ_CONFIG_CAPABILITY(CONFIG_ITEM_NAME)

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Subscriber #1">;

#include "data.h"
#include "sandbox.h"

// Current config value
Data *configData = nullptr;

//
// Process a change in config data
//
void process_update(ConfigItem *config)
{
	if (config->data != nullptr)
	{
		if (sandbox_validate(config->data) < 0)
		{
			Debug::log("thread {} Validation failed for {}",
			           thread_id_get(),
			           config->data);
		}
		else
		{
			// New value is valid - release our claim on the old value
			if (configData != nullptr)
			{
				free(configData);
			}
			configData = static_cast<Data *>(config->data);

			// Claim the new value so we keep access to it even if
			// the next value from the broker is invalid
			heap_claim(MALLOC_CAPABILITY, configData);

			// Act on the new value
			;
		}
	}

	// Print the current value
	print_config(CONFIG_ITEM_NAME, configData);
}

//
// Thread entry point.
//
void __cheri_compartment("subscriber1") init()
{
	// Initial read of the config to get the current value (if any)
	// and the version & futex to wait on
	auto config = get_config(READ_CONFIG_CAPABILITY(CONFIG_ITEM_NAME));
	if (config == nullptr)
	{
		Debug::log(
		  "thread {} failed to get {}", thread_id_get(), CONFIG_ITEM_NAME);
		return;
	}
	Debug::log("thread {} got version:{} of {}",
	           thread_id_get(),
	           config->version,
	           CONFIG_ITEM_NAME);
	process_update(config);
	auto configVersion = config->version;

	// Loop waiting for config changes
	while (true)
	{
		futex_wait(&(config->version), configVersion);
		auto config = get_config(READ_CONFIG_CAPABILITY(CONFIG_ITEM_NAME));
		Debug::log("thread {} got version:{} of {}",
		           thread_id_get(),
		           config->version,
		           CONFIG_ITEM_NAME);

		process_update(config);
		configVersion = config->version;

		// Check we're not leaking data;
		// Debug::log("heap quota available: {}",
		//   heap_quota_remaining(MALLOC_CAPABILITY));
	}
}

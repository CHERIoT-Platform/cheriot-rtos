// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <thread.h>
#include <tick_macros.h>

// Compartment can set config values config1 and config2
#include "config_broker.h"
DEFINE_CONFIG_SOURCE_CAPABILITY("config1", "config2")

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Config Source ">;

#include "data.h"
#include "sleep.h"

// Helper to create some dummy config
Data *gen_config(int count, const char *token)
{
	Data *d  = static_cast<Data *>(malloc(sizeof(Data)));
	d->count = count;
	for (auto i = 0; i < 7; i++)
	{
		d->token[i] = token[i];
	};
	return d;
};

// Hack function to generate some configuration  data
void __cheri_compartment("config_source") init()
{
	// Hack for now to initialise the config data
	Data *d;
	d = gen_config(0, "Wile-E");
	Debug::log("thread {} Set config1", thread_id_get());
	set_config(CONFIG_CAPABILITY, "config1", static_cast<void *>(d));
	free(d);

	d = gen_config(0, "Coyote");
	Debug::log("thread {} Set config2", thread_id_get());
	set_config(CONFIG_CAPABILITY, "config2", static_cast<void *>(d));
	free(d);

	int loop = 1;
	while (true)
	{
		sleep(1500);
		Debug::log("thread {} Set config1", thread_id_get());
		d = gen_config(loop++, "Wile-E");
		set_config(CONFIG_CAPABILITY, "config1", static_cast<void *>(d));
		free(d);

		sleep(1500);
		Debug::log("thread {} Set config2", thread_id_get());
		d = gen_config(loop++, "Coyote");
		set_config(CONFIG_CAPABILITY, "config2", static_cast<void *>(d));
		free(d);

		// Check we're not leaking data;
		// Debug::log("heap quota available: {}",
		//   heap_quota_remaining(MALLOC_CAPABILITY));

		// Give the compartments a chance to print their
		// config values from timers
		sleep(3000);
	}
}

// Entry point for a thread that periodically generates
// invalid configuration data
void __cheri_compartment("config_source") bad_dog()
{
	while (true)
	{
		sleep(12000);

		Debug::log("thread {} Sending bad data for config1", thread_id_get());
		void *d = malloc(4);
		set_config(CONFIG_CAPABILITY, "config1", d);
		free(d);
	}
}

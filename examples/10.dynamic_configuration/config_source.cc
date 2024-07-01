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

// Helper to set some dummy config
void gen_config(const char *name, int count, const char *token)
{
	Data *d  = static_cast<Data *>(malloc(sizeof(Data)));
	d->count = count;
	for (auto i = 0; i < 7; i++)
	{
		d->token[i] = token[i];
	};
	Debug::log("thread {} Set {}", thread_id_get(), name);
	set_config(CONFIG_CAPABILITY, name, static_cast<void *>(d));
	free(d);
};

void gen_bad_config(const char * name)
{
	Debug::log("thread {} Sending bad data for {}", thread_id_get(), name);
	void *d = malloc(4);
	set_config(CONFIG_CAPABILITY, name, d);
	free(d);
};



// Hack function to generate some configuration  data
void __cheri_compartment("config_source") init()
{
	gen_config("config1", 0, "Wile-E");
	gen_config("config2", 0, "Coyote");

	int loop = 1;
    while (true)      
	{
		sleep(1500);
		gen_config("config1", loop++, "Wile-E");

		sleep(1500);
		gen_config("config2", loop++, "Coyote");
               
		// Check we're not leaking data;
		// Debug::log("heap quota available: {}",
		//   heap_quota_remaining(MALLOC_CAPABILITY));
 
		// Give the compartments a chance to print their
		// config values from timers
		sleep(3000);
	}
};

void __cheri_compartment("config_source") bad_dog()
{
    while (true)
    {
		sleep(12000);
		gen_bad_config("config1");
	}
};

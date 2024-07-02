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

#define CONFIG1 "config1"
DEFINE_READ_CONFIG_CAPABILITY(CONFIG1)

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Subscriber #1">;

#include "data.h"
#include "sleep.h"

// Local pointer to the most recent config value;
Data *config = nullptr;

//
// Callback to handle config updates
//
void __cheri_callback update(const char *id, void *data)
{
	// Commeting out the following line causes a tag violation
	// to be generated in the config broker as during the call
	// to get_config() in init() below, but this callback isn't
	// used or referenced anymore !

	print_config("Update", id, config);
}


//
// Thread entry point.
//
void __cheri_compartment("subscriber1") init()
{
	// Register to get config updates
	Debug::log("thread {} Register for config updates", thread_id_get());
	auto c = get_config(READ_CONFIG_CAPABILITY(CONFIG1));
	Debug::log("thread {} got {}", thread_id_get(), c);
	
	// Loop printing our config value occasionally
	while (true)
	{
		sleep(4500);
		print_config("Timer ", "config1", config);
	}
	
}

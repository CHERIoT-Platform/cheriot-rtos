// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <compartment.h>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <multiwaiter.h>
#include <thread.h>

// Define a sealed capability that gives this compartment
// read access to configuration data "config1" and "config2"
#include "config_broker.h"
#include "token.h"
#define CONFIG1 "config1"
DEFINE_READ_CONFIG_CAPABILITY(CONFIG1)
#define CONFIG2 "config2"
DEFINE_READ_CONFIG_CAPABILITY(CONFIG2)

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Subscriber #3">;

#include "data.h"
#include "sandbox.h"

// Keep track of the items and thier last version
struct Config
{
	const char *name;      // Name of the item
	SObj        capablity; // Sealed Read Capability
	ConfigItem *item;      // last received item from broker
	Data       *data;      // last valid config data
};

//
// Process a change in config data
//
void process_update(Data **configData, ConfigItem *config, const char *itemName)
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
			if (*configData != nullptr)
			{
				free(*configData);
			}
			*configData = static_cast<Data *>(config->data);

			// Claim the new value so we keep access to it even if
			// the next value from the broker is invalid
			heap_claim(MALLOC_CAPABILITY, *configData);

			// Act on the new value
			;
		}
	}

	// Print the current value
	print_config(itemName, *configData);
}

//
// Thread entry point.
//
void __cheri_compartment("subscriber3") init()
{
	// List of configuration items we are tracking
	Config configItems[] = {
	  {CONFIG1, READ_CONFIG_CAPABILITY(CONFIG1), nullptr, nullptr},
	  {CONFIG2, READ_CONFIG_CAPABILITY(CONFIG2), nullptr, nullptr},
	};

	auto numOfItems = sizeof(configItems) / sizeof(configItems[0]);

	// Create the multi waiter
	struct MultiWaiter *mw = nullptr;
	Timeout             t1{MS_TO_TICKS(1000)};
	multiwaiter_create(&t1, MALLOC_CAPABILITY, &mw, 3);
	if (mw == nullptr)
	{
		Debug::log("thread {} failed to create multiwaiter", thread_id_get());
		return;
	}

	// Initial read of the config item to get the current value (if any)
	// and the version & futex to wait on
	for (auto &c : configItems)
	{
		c.item = get_config(c.capablity);
		if (c.item == nullptr)
		{
			Debug::log("thread {} failed to get {}", thread_id_get(), c.name);
			return;
		}
		Debug::log("thread {} got version:{} of {}",
		           thread_id_get(),
		           c.item->version,
		           c.name);
		process_update(&c.data, c.item, c.name);
	}

	// Loop waiting for config changes
	while (true)
	{
		// Create a set of wait events
		struct EventWaiterSource events[numOfItems];

		for (auto i = 0; i < numOfItems; i++)
		{
			events[i] = {&(configItems[i].item->version),
			             EventWaiterFutex,
			             configItems[i].item->version};
		}

		Timeout t{MS_TO_TICKS(10000)};
		if (multiwaiter_wait(&t, mw, events, 2) == 0)
		{
			// find out which value changed
			for (auto i = 0; i < numOfItems; i++)
			{
				if (events[i].value == 1)
				{
					auto c  = &configItems[i];
					c->item = get_config(c->capablity);
					Debug::log("thread {} got version:{} of {}",
					           thread_id_get(),
					           c->item->version,
					           c->name);

					process_update(&c->data, c->item, c->name);
				}
			}
		}
		else
		{
			Debug::log("thread {} wait timeout", thread_id_get());
		}

		// Check we're not leaking data;
		// Debug::log("heap quota available: {}",
		//   heap_quota_remaining(MALLOC_CAPABILITY));
	}
}

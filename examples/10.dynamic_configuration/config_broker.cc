// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include "cdefs.h"
#include "cheri.hh"
#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
#include <fail-simulator-on-error.h>
#include <futex.h>
#include <thread.h>

#include <string.h>
#include <vector>

#include "config_broker.h"

// Import some useful things from the CHERI namespace.
using namespace CHERI;

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Config Broker ">;

//
// Data type of config data for a compartment
//
struct cbInfo
{
	uint16_t id;
	__cheri_callback void (*cb)(const char *, void *);
};

struct Config
{
	char                        *name;
	bool                         updated;
	std::vector<struct cbInfo *> cbList;
	void                        *data;
};

//
// futex to signal when new data is available
//
static uint32_t pending = 0;

//
// Set of config data items.
//
std::vector<struct Config *> configData;

//
// unseal a config capability.
//
ConfigToken *config_capability_unseal(SObj sealed_cap)
{
	auto            key    = STATIC_SEALING_TYPE(ConfigKey);
	static uint16_t nextId = 1;

	ConfigToken *token =
	  token_unseal<ConfigToken>(key, Sealed<ConfigToken>{sealed_cap});

	if (token == nullptr)
	{
		Debug::log("invalid config capability {}", sealed_cap);
		return nullptr;
	}

	if (token->id == 0)
	{
		// Assign an ID so we can track the callbacks added
		// from this capability
		token->id = nextId++;

		// Count how many config ids are defined to
		// save us having to do this each time
		for (auto i = 0; i < MAX_CONFIG_IDS; i++)
		{
			if (strlen(token->configId[i]))
			{
				token->count++;
			}
		}
	}

	return token;
}

//
// Find a Config by name.  If it doesn't already exist
// create one.
//
Config *find_config(const char *name)
{
	for (auto &c : configData)
	{
		if (strcmp(c->name, name) == 0)
		{
			return c;
		}
	}

	// Allocate a Config object
	Config *c = static_cast<Config *>(malloc(sizeof(Config)));

	// Save the name
	c->name = static_cast<char *>(malloc(strlen(name)));
	strncpy(c->name, name, strlen(name));

	c->updated = false;
	c->data    = nullptr;

	// Add it to the vector
	configData.push_back(c);

	return c;
};

//
// Add a callback to the list for a Config.  Use the
// id to prevent adding more than one callback for
// each client
//
void add_callback(Config               *c,
                  uint16_t              id,
                  __cheri_callback void cb(const char *, void *))
{
	for (auto &cb_info : c->cbList)
	{
		if (cb_info->id == id)
		{
			cb_info->cb = cb;
			return;
		}
	}

	cbInfo *cb_info = static_cast<cbInfo *>(malloc(sizeof(cbInfo)));
	cb_info->id     = id;
	cb_info->cb     = cb;
	c->cbList.push_back(cb_info);
}

//
// Set a configuration data item.  The capabailty must have the
// is_source set to true and include the name of the item being
// set
//
void __cheri_compartment("config_broker")
  set_config(SObj sealed_cap, const char *name, void *data)
{
	ConfigToken *token = config_capability_unseal(sealed_cap);

	if (!token->is_source)
	{
		Debug::log("Not a source capability: {}", sealed_cap);
		return;
	}

	bool valid = false;
	for (auto i = 0; i < token->count; i++)
	{
		if (strcmp(token->configId[i], name) == 0)
		{
			valid = true;
			break;
		}
	}
	if (!valid)
	{
		Debug::log("Not a source capability for: {} {}", sealed_cap, name);
		return;
	}

	// See if we already have a config structure
	Config *c = find_config(name);

	// Free any claim we had on the previous data
	if (c->data)
	{
		heap_free(MALLOC_CAPABILITY, c->data);
	}

	// Create a read only version of the capability to pass to
	// the subscribers, and add a claim so we can deliver it
	// when a compartment registers a callback.
	CHERI::Capability readOnlyData{data};
	readOnlyData.permissions() &=
	  {CHERI::Permission::Load, CHERI::Permission::Global};
	c->data = readOnlyData;
	heap_claim(MALLOC_CAPABILITY, c->data);

	// Mark it as having been updated
	c->updated = true;

	pending++;
	futex_wake(&pending, -1);
}

//
// Register to get config data for a compartment.  The callback
// function will be called whenever an item listed in the capability
// changes.  If data is already available then the callback
// will be called immediately.
//
void __cheri_compartment("config_broker")
  on_config(SObj sealed_cap, __cheri_callback void cb(const char *, void *))
{
	// Get the calling compartments name from
	// its sealed capability
	ConfigToken *token = config_capability_unseal(sealed_cap);

	if (token == nullptr)
	{
		Debug::log("Invalid id");
		return;
	}

	for (auto i = 0; i < token->count; i++)
	{
		Debug::log("thread {} on_config called for {} by id {}",
		           thread_id_get(),
		           static_cast<const char *>(token->configId[i]),
		           token->id);

		auto c = find_config(token->configId[i]);
		add_callback(c, token->id, cb);
		if (c->data)
		{
			cb(token->configId[i], c->data);
		}
	}
}

//
// thread enrty point
//
void __cheri_compartment("config_broker") init()
{
	while (true)
	{
		// wait for updates
		futex_wait(&pending, 0);
		Debug::log("thread {} processing {} updates", thread_id_get(), pending);

		pending = 0;

		// Procces any the modified config data
		//
		// Two timing considerations for events that could
		// occur whilst were making the callbacks
		//
		// - If a new callback is registered they get called
		//   directly, so they might get called twice, which is OK
		//
		// - If a new data value is suppled then the rest of the
		//   callbacks will be called with the new value. The item
		//   will be tagged as updated and pending incremented so
		//   we also pick it up on the next loop. Some callbacks
		//   might be called twice with the same value, which is OK.
		//
		for (auto &c : configData)
		{
			if (c->updated)
			{
				c->updated = false;
				// Call all the callbacks
				for (auto &cb_info : c->cbList)
				{
					cb_info->cb(c->name, c->data);
				}
			}
		}
	}
}
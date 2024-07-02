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
#include "sandbox.h"

// Import some useful things from the CHERI namespace.
using namespace CHERI;

// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<DEBUG_CONFIG_BROKER, "Config Broker">;

// Internal represention of Configurtaion Items
struct NamedConfigItem
{
	char      *name; // name matched to capabilities
	ConfigItem item;
};

//
// Set of config data items.
//
std::vector<struct NamedConfigItem *> configData;

//
// unseal a config capability.
//
ConfigToken *config_capability_unseal(SObj sealedCap)
{
	auto key = STATIC_SEALING_TYPE(ConfigKey);

	ConfigToken *token =
	  token_unseal<ConfigToken>(key, Sealed<ConfigToken>{sealedCap});

	if (token == nullptr)
	{
		Debug::log("invalid config capability {}", sealedCap);
		return nullptr;
	}

	Debug::log("Unsealed id: {} kind: {} size:{} item: {}",
	           token->id,
	           token->kind,
	           token->maxSize,
	           token->ConfigId);

	if (token->id == 0)
	{
		// Assign an ID so we can track the callbacks added
		// from this capability
		static uint16_t nextId = 1;
		token->id              = nextId++;
	}

	return token;
}

//
// Find a Config by name.  If it doesn't already exist
// create one.
//
NamedConfigItem *find_or_create_config(const char *name)
{
	for (auto &c : configData)
	{
		if (strcmp(c->name, name) == 0)
		{
			return c;
		}
	}

	// Allocate a Config object
	NamedConfigItem *c =
	  static_cast<NamedConfigItem *>(malloc(sizeof(NamedConfigItem)));

	// Save the name as a read only capability
	auto nameBuffer = static_cast<char *>(malloc(strlen(name)));
	strncpy(nameBuffer, name, strlen(name));
	CHERI::Capability roName{nameBuffer};
	roName.permissions() &=
	  roName.permissions().without(CHERI::Permission::Store);
	c->name         = roName;
	c->item.version = 0;
	c->item.data    = nullptr;

	// Add it to the vector
	configData.push_back(c);

	return c;
};

//
// Set a new value for the configuration item described by
// the capability.
//
int __cheri_compartment("config_broker")
  set_config(SObj sealedCap, void *data, size_t size)
{
	Debug::log("thread {} Set config called with {} {} {}",
	           thread_id_get(),
	           sealedCap,
	           data,
	           size);

	ConfigToken *token = config_capability_unseal(sealedCap);
	if (token == nullptr)
	{
		Debug::log("Invalid capability: {}", sealedCap);
		return -1;
	}

	// Check we have a WriteToken
	if (token->kind != WriteToken)
	{
		Debug::log(
		  "Not a write capability for {}: {}", token->ConfigId, sealedCap);
		return -1;
	}

	// Check the size and data are consistent with the token
	// and each other.
	if (size > token->maxSize)
	{
		Debug::log("invalid size {} for capability: {}", size, sealedCap);
		return -1;
	}

	if (size > static_cast<size_t>(Capability{data}.bounds()))
	{
		Debug::log("size {} > data.bounds() {}", size, data);
		return -1;
	}

	// Allocate heap space for the new value
	void *newData = malloc(size);
	if (newData == nullptr)
	{
		Debug::log("Failed to allocate space for {}", token->ConfigId);
		return -1;
	}

	// Even though we've done the obvious checks were paranoid about
	// the incomming data so do the copy in a separate compartment
	if (sandbox_copy(data, newData, size) < 0)
	{
		Debug::log("Data copy failed from {} to {}", data, newData);
		free(newData);
		return -1;
	};

	// Find or create a config structure
	NamedConfigItem *c = find_or_create_config(token->ConfigId);

	// Free the old data value.  Any subscribers that received it should
	// have thier own claim on it if needed
	if (c->item.data)
	{
		free(c->item.data);
	}

	// Neither we nor the subscribers need to be able to update the
	// value, so just track through a readOnly capabaility
	CHERI::Capability roData{newData};
	roData.permissions() &=
	  roData.permissions().without(CHERI::Permission::Store);
	c->item.data = roData;
	Debug::log("Data {}", c->item.data);

	// Mark it as having been updated
	c->item.version++;

	// Trigger out thread to process the update
	Debug::log("Waking subscribers {}", c->item.version);
	futex_wake(&(c->item.version), -1);

	return 0;
}

//
// Get the current value of a Configuration item.  The data
// member will be nullptr if the item has not yet been set.
// The version member can be used as a futex to wait for changes.
//
ConfigItem *__cheri_compartment("config_broker") get_config(SObj sealedCap)
{
	Debug::log(
	  "thread {} get_config called with {}", thread_id_get(), sealedCap);

	// Get the calling compartments name from
	// its sealed capability
	ConfigToken *token = config_capability_unseal(sealedCap);

	if (token == nullptr)
	{
		Debug::log("Invalid capability {}", sealedCap);
		return nullptr;
	}

	auto c = find_or_create_config(token->ConfigId);

	// return a read only copy of the item;
	// Debug::log("was {}", c);
	CHERI::Capability item{&c->item};
	item.permissions() &= item.permissions().without(CHERI::Permission::Store);
	return item;
}

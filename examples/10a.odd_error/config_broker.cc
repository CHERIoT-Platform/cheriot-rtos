// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include "cdefs.h"
#include "cheri.hh"
#include <compartment.h>
#include <cstdlib>
#include <debug.hh>
//#include <fail-simulator-on-error.h>
#include <futex.h>
#include <thread.h>

#include <string.h>
#include <vector>

#include "config_broker.h"

// Import some useful things from the CHERI namespace.
using namespace CHERI;

// Expose debugging features unconditionally for this compartment.
//using Debug = ConditionalDebug<DEBUG_CONFIG_BROKER, "Config Broker">;
using Debug = ConditionalDebug<true, "Config Broker">;

//
// count of un-sent updates; used as a futex
//
static uint32_t pending = 0;

//
// Set of config data items.
//
std::vector<struct ConfigItem *> configData;

using namespace CHERI;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	auto [exceptionCode, registerNumber] = extract_cheri_mtval(mtval);
	void *faultingRegister               = nullptr;
	if (registerNumber == RegisterNumber::PCC)
	{
		faultingRegister = frame->pcc;
	}
	else if ((registerNumber > RegisterNumber::CZR) &&
	         (registerNumber <= RegisterNumber::CA5))
	{
		// The registers array does not include cnull.
		faultingRegister = frame->registers[int(registerNumber) - 1];
	}
	// Make sure that we have a new line before the debug output.
	// This uses the UART driver directly to write a single byte.
	MMIO_CAPABILITY(Uart, uart)->blocking_write('\n');

	Debug::log("Detected {} in Conifg Broker.  Register {} contained "
	           "invalid value: {}",
	           exceptionCode,
	           registerNumber,
	           faultingRegister);
	return ErrorRecoveryBehaviour::ForceUnwind;
}
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
	           token->configId);

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
ConfigItem *find_or_create_config(const char *name)
{
	for (auto &c : configData)
	{
		if (strcmp(c->name, name) == 0)
		{
			Debug::log("Found it");
			return c;
		}
	}

	// Allocate a Config object
	ConfigItem *c = static_cast<ConfigItem *>(malloc(sizeof(ConfigItem)));
	Debug::log("Created {} {}", name, c);

	// Save the name
	c->name = static_cast<char *>(malloc(strlen(name)));
	strncpy(c->name, name, strlen(name));
	Debug::log("Name copied");
	//CHERI::Capability(c->name).permissions() &=
	//  {CHERI::Permission::Load};

	c->version = 0;
	c->data    = nullptr;

	// Add it to the vector
	Debug::log("Adding to vector {}", c);
	configData.push_back(c);

	Debug::log("returning {}", c);
	return c;
};


//
// Set a new value for the configuration item described by
// the capability.
//
int __cheri_compartment("config_broker")
  set_config(SObj sealedCap, void *data, size_t size)
{
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
		  "Not a write capability for {}: {}", token->configId, sealedCap);
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

	// Find or create a config structure
	ConfigItem *c = find_or_create_config(token->configId);

	// Allocate heap space for the new value
	void *newData = malloc(size);
	if (newData == nullptr)
	{
		Debug::log("Failed to allocate space for {}", token->configId);
		return -1;
	}

	// If we were paranoid about the incomming data we could make this
	// something that we call into a separate compartment to do 
	memcpy(newData, data, size);

	// Free the old data value.  Any subscribers that received it should
	// have thier own claim on it if needed
	if (c->data)
	{
		free(c->data);
	}

	// Neither we nor the subscribers need to be able to update the
	// value, so just track through a readOnly capabaility
	c->data = newData;
	CHERI::Capability(c->data).permissions() &=
	  {CHERI::Permission::Load, CHERI::Permission::Global};

	// Mark it as having been updated
	c->version++;

	// Trigger out thread to process the update 
	futex_wake(&c->version, -1);

	return 0;
}

//
// Get the current value of a Configuration item.  The data
// member will be nullptr if the item has not yet been set. 
// The version member can be used as a futex to wait for changes.
//
ConfigItem *__cheri_compartment("config_broker")
  get_config(SObj sealedCap)
{
	// Get the calling compartments name from
	// its sealed capability
	ConfigToken *token = config_capability_unseal(sealedCap);

	if (token == nullptr)
	{
		Debug::log("Invalid capability {}", sealedCap);
		return nullptr;
	}

	Debug::log("thread {} on_config called for {} by id {}",
	           thread_id_get(),
	           static_cast<const char *>(token->configId),
	           token->id);

	auto c = find_or_create_config(token->configId);
	Debug::log("Created");

	// return a read only copy of the item;
	auto item = c;
	CHERI::Capability(item).permissions() &=
	  {CHERI::Permission::Load, CHERI::Permission::Global};
	return item;
}

/*
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
				for (auto &cbInfo : c->cbList)
				{
					cbInfo->cb(c->name, c->data);
				}
			}
		}
	}
}
*/
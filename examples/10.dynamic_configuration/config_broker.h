// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include "cdefs.h"
#include "compartment-macros.h"
#include "token.h"
#include <compartment.h>

typedef char ConfigId[16];
#define MAX_CONFIG_IDS 8

struct ConfigToken
{
	bool     is_source;
	uint16_t id;
	uint8_t  count;
	ConfigId configId[MAX_CONFIG_IDS];
};

#define DECLARE_CONFIG_CAPABILITY(name)                                        \
	DECLARE_STATIC_SEALED_VALUE(                                               \
	  struct ConfigToken, config_broker, ConfigKey, name);

#define DEFINE_CONFIG_CAPABILITY(...)                                          \
	DEFINE_STATIC_SEALED_VALUE(struct ConfigToken,                             \
	                           config_broker,                                  \
	                           ConfigKey,                                      \
	                           __config_capability,                            \
	                           false,                                          \
	                           0,                                              \
	                           0,                                              \
	                           __VA_ARGS__);

#define DEFINE_CONFIG_SOURCE_CAPABILITY(...)                                   \
	DEFINE_STATIC_SEALED_VALUE(struct ConfigToken,                             \
	                           config_broker,                                  \
	                           ConfigKey,                                      \
	                           __config_capability,                            \
	                           true,                                           \
	                           0,                                              \
	                           0,                                              \
	                           __VA_ARGS__);

DECLARE_CONFIG_CAPABILITY(__config_capability)

#define CONFIG_CAPABILITY STATIC_SEALED_VALUE(__config_capability)

/**
 * Register a callback to get notification of configuration
 * changes.
 */
void __cheri_compartment("config_broker")
  on_config(SObj cap, __cheri_callback void cb(const char *, void *));

/**
 * Set configuration data
 */
void __cheri_compartment("config_broker")
  set_config(SObj cap, const char *name, void *data);

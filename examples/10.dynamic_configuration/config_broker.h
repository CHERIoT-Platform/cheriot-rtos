// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include "cdefs.h"
#include "compartment-macros.h"
#include "token.h"
#include <compartment.h>

typedef char ConfigId[8];
#define MAX_CONFIG_IDS 8

struct ConfigToken
{
	bool     is_source;
	uint16_t id;
	uint8_t  count;
	ConfigId configId;
	//int configId;
};

/*
#define DECLARE_CONFIG_CAPABILITY(name)                                        \
	DECLARE_STATIC_SEALED_VALUE(                                               \
	  struct ConfigToken, config_broker, ConfigKey, name);
*/

#define DEFINE_CONFIG_CAPABILITY(name)                                         \
                                                                               \
	DECLARE_STATIC_SEALED_VALUE(                                               \
	  struct ConfigToken, config_broker, ConfigKey, __config_capability_ ## name);                     \
                                                                               \
	DEFINE_STATIC_SEALED_VALUE(struct ConfigToken,                             \
	                           config_broker,                                  \
	                           ConfigKey,                                      \
	                           __config_capability_ ## name,                     \
	                           false,                                          \
	                           0,                                              \
	                           0,                                              \
	                           name);

#define DEFINE_CONFIG_SOURCE_CAPABILITY(name)                                   \
                                                                               \
	DECLARE_STATIC_SEALED_VALUE(                                               \
	  struct ConfigToken, config_broker, ConfigKey, __config_capability_ ## name);                     \
                                                                               \
	DEFINE_STATIC_SEALED_VALUE(struct ConfigToken,                             \
	                           config_broker,                                  \
	                           ConfigKey,                                      \
	                           __config_capability_ ## name,                            \
	                           true,                                           \
	                           0,                                              \
	                           0,                                              \
	                           name);

/*
DECLARE_CONFIG_CAPABILITY(__config_capability)
*/


#define CONFIG_CAPABILITY(name) STATIC_SEALED_VALUE(__config_capability_ ## name)

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
  set_config(SObj cap, void *data);

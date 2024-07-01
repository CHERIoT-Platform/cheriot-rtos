// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include "cdefs.h"
#include "compartment-macros.h"
#include "token.h"
#include <compartment.h>

struct ConfigToken
{
	bool       is_source;
	uint16_t   id;
	const char configId[];
};

#define DEFINE_CONFIG_CAPABILITY(name)                                         \
                                                                               \
	DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(                                    \
	  struct {                                                                 \
		  bool       is_source;                                                \
		  uint16_t   id;                                                       \
		  const char configId[sizeof(name)];                                   \
	  },                                                                       \
	  config_broker,                                                           \
	  ConfigKey,                                                               \
	  __config_capability_##name,                                              \
	  false,                                                                   \
	  0,                                                                       \
	  name);

#define DEFINE_CONFIG_SOURCE_CAPABILITY(name)                                  \
                                                                               \
	DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(                                    \
	  struct {                                                                 \
		  bool       is_source;                                                \
		  uint16_t   id;                                                       \
		  const char configId[sizeof(name)];                                   \
	  },                                                                       \
	  config_broker,                                                           \
	  ConfigKey,                                                               \
	  __config_capability_##name,                                              \
	  true,                                                                    \
	  0,                                                                       \
	  name);

#define CONFIG_CAPABILITY(name) STATIC_SEALED_VALUE(__config_capability_##name)

/**
 * Register a callback to get notification of configuration
 * changes.
 */
void __cheri_compartment("config_broker")
  on_config(SObj cap, __cheri_callback void cb(const char *, void *));

/**
 * Set configuration data
 */
void __cheri_compartment("config_broker") set_config(SObj cap, void *data);

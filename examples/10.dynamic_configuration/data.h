// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <debug.hh>
#include <thread.h>

// In this example for simplicity all configuration
// items have the same data structure.  In a real
// system they would be different stuctures each
// with their own validators
struct Data
{
	uint32_t count;
	uint32_t padding;
	char     token[8];
};

// Helper fucntion for the example to print a config item
static inline void print_config(const char *why, const char *name, Data *d)
{
	if (d == nullptr)
	{
		Debug::log("thread {} {} {} -- No config yet", why, name);
	}
	else
	{
		Debug::log("thread {} {} {} -- config count: {} token: {}",
		           thread_id_get(),
		           why,
		           name,
		           d->count,
		           static_cast<const char *>(d->token));
	}
}
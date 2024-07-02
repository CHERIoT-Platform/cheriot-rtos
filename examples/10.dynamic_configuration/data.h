// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <stdlib.h>

// In this example for simplicity all configuration
// items have the same data structure.  In a real
// system they would be different stuctures each
// with their own validators
struct Data
{
	uint32_t count;
	uint32_t padding;
	char     token[16];
};

// Helper function for the example to print a config item
void __cheri_libcall print_config(const char *name, Data *d);

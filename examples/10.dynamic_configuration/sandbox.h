// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

// Contributed by Configured Things Ltd

#include <stdlib.h>

//
// Run a validator data in sandpit compartment
//
int __cheri_compartment("sandbox") sandbox_validate(void *data);

int __cheri_compartment("sandbox")
  sandbox_copy(void *src, void *dst, size_t size);
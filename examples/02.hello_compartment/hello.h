#include <compartment.h>
#include <errno.h>

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * Write `msg` to the default uart.
 */
int __cheri_compartment("uart") write(const char *msg);

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>

/**
 * Write `msg` to the default uart.
 */
int __cheri_compartment("uart") write(const char *msg);

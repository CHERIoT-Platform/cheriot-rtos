#include <compartment.h>
// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

/**
 * Write `msg` to the default uart.
 */
void __cheri_compartment("uart") write(const char *msg);

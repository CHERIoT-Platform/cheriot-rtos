// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdlib>

int __cheri_compartment("consumer") set_queue(SObjStruct *queueHandle);

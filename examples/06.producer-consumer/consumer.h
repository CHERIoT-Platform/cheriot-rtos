// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <cstdlib>

void __cheri_compartment("consumer") set_queue(SObjStruct *queueHandle);

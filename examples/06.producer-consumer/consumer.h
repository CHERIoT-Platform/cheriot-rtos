// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <queue.h>

void __cheri_compartment("consumer")
  set_queue(CHERI_SEALED(struct MessageQueue *) queueHandle);

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <queue.h>

int __cheri_compartment("consumer")
  set_queue(CHERI_SEALED(struct MessageQueue *) queueHandle);

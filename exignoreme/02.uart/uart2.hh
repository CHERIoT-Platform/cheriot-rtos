#pragma once
// uart2.hh

#include <compartment.h>
#include <queue.h>

// function declarations
void __cheriot_compartment("uart") set_queue(CHERI_SEALED(MessageQueue *) newQueue);

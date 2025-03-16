#pragma once
// uart2.hh

#include <compartment.h>
#include <queue.h>

typedef struct QueueMessage {
    uint32_t messType;
    uint32_t messData;
} QueueMessage;

// function declarations
int __cheriot_compartment("uart") set_queue(CHERI_SEALED(MessageQueue *) newQueue);

#pragma once
// uart.hh

#include <cstdint>
#include <queue.h>

// function declarations
uint16_t append_to_tx_buffer(char* msg, uint16_t len);

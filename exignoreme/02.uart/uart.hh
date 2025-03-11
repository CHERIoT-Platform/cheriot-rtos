#pragma once
// uart.hh

#include <cstddef>
#include <cstdint>

// function declarations
uint16_t append_to_tx_buffer(char* msg, uint16_t len);
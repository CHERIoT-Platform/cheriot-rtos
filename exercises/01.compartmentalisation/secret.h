#pragma once
#include <stdint.h>

/**
 * Set the secret the first time.
 *
 * This blocks until there is a character available on the UART.  This is
 * *not* a good way of getting entropy, but it's the only one that we have
 * in the simulator.
 */
void set_secret();

/**
 * Report the current value of the secret and pick a new one.
 */
void check_secret(int32_t guess);

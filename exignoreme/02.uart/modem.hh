#pragma once
// Modem.hh

#include <cstddef>

// function declarations
extern void tasks_process();
extern void process_serial_replies(char *buffer, size_t len);
extern void tasks_set_initialise_modem();   // Sets up our modem with to talk to our server.

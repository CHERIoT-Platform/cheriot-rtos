#pragma once
// Modem.hh

#include <cstddef>
#include <cstdint>

// function declarations
extern void tasks_process();
extern bool process_serial_replies(char *buffer, size_t len);
extern void tasks_set_initialise_modem();   // Sets up our modem with to talk to our server.
void tasks_send_message(char *msg); // Send messages. This puts the data in teh URL rather than the body.
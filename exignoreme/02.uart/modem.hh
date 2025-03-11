#pragma once
// Modem.hh

#include <cstddef>
// #include <cstdint>

// function declarations
// extern void modem_init();
// extern int modem_serial_send(const char *msg, size_t len);
// extern int modem_serial_read(char *msg, size_t len);
// extern uint16_t modem_rx_buff_len();
extern void tasks_process();
// extern size_t modem_process_serial();

// Ones we're defineitely going to need.
extern void process_serial_replies(char *buffer, size_t len);
extern void tasks_set_initialise_modem();   // Sets up our modem with to talk to our server.

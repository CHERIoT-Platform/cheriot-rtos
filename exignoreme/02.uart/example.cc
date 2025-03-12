// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment-macros.h"
#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <stdio.h>
#include <platform/sunburst/platform-gpio.hh>
#include <platform/sunburst/platform-pinmux.hh>
#include <platform/sunburst/platform-uart.hh>
#include "modem.hh"

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "Main compartment">;

#define RX_BUFFER_SIZE (20)
void rx_all() {
	char rxBuff[RX_BUFFER_SIZE + 1];
	rxBuff[20] = 0x00;
	int toRead = modem_rx_buff_len();
	if(toRead > RX_BUFFER_SIZE) {
		toRead = RX_BUFFER_SIZE;
	}
	if(toRead > 0) {
		uint16_t ret = modem_serial_read(rxBuff, toRead);
		Debug::log("Bytes read = {}", ret);
		rxBuff[ret] = 0;	// Double check termination
		if(ret > 0) {
			printf("Rx: %s\r\n", rxBuff);
		}
	}
}

/// Thread entry point.
void __cheri_compartment("main_comp") main_entry()
{
	// Print welcome, along with the compartment's name to the default UART.
	Debug::log("Sonata Simple AT Command tests");

	// Setting up the pinmux.
	auto pinSinks = MMIO_CAPABILITY(SonataPinmux::PinSinks, pinmux_pins_sinks);
	pinSinks->get(SonataPinmux::PinSink::pmod0_2).select(4); // uart1 tx -> pmod0_2
	auto blockSinks = MMIO_CAPABILITY(SonataPinmux::BlockSinks, pinmux_block_sinks);
	blockSinks->get(SonataPinmux::BlockSink::uart_1_rx).select(5); // pmod0_3 -> uart1 rx

	modem_init();

	int ret = 0;
	ret = modem_serial_send("at+cimi\r\n", 9);
	// rx_all();
	// rx_all();
	// rx_all();
	// rx_all();
	// thread_millisecond_wait(1000);
	// rx_all();
	// rx_all();
	// rx_all();
	// rx_all();
	// thread_millisecond_wait(1000);
	// rx_all();
	// rx_all();
	// rx_all();
	// rx_all();
	// thread_millisecond_wait(1000);
	// rx_all();
	// rx_all();
	// rx_all();
	// rx_all();
	// thread_millisecond_wait(1000);
	// rx_all();
	// rx_all();
	// rx_all();
	// rx_all();
	// thread_millisecond_wait(1000);
	// rx_all();
	// rx_all();
	// rx_all();
	// rx_all();

	// Debug::log("Setup the modem");
	// tasks_set_initialise_modem();
	Debug::log("Start main loop");
	while(true) {
		// tasks_process();
		// thread_millisecond_wait(5000);
		// modem_process_serial();
		thread_millisecond_wait(5000);
		// rx_all();
	}
}

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment-macros.h"
#include "platform/sunburst/platform-pinmux.hh"
#include <compartment.h>
#include <cstdint>
#include <debug.hh>
#include <futex.h>
#include <interrupt.h>
#include <tick_macros.h>
#include <timeout.h>
#include <platform/sunburst/platform-uart.hh>
#include <stdio.h>
#include "modem.hh"


/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "uart">;

// DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(uart1InterruptCap, Uart1Interrupt, true, true);
#define BUFF_OUTPUT_SIZE	(200)
#define BUFF_INPUT_SIZE		(40)

char outputBuffer[BUFF_OUTPUT_SIZE + 1];
uint16_t outputBufferOffset = 0;
uint16_t outputBufferLength = 0;

char inputBuffer[BUFF_INPUT_SIZE + 1];
uint16_t inputBufferOffset = 0;

uint16_t append_to_tx_buffer(char* msg, uint16_t len)
{
	if((outputBufferLength + len) > BUFF_OUTPUT_SIZE) {	// It's too big - we must limit instead.
		Debug::log("Tx buffer truncating! Current {} of {}. Attempting to write {}.", outputBufferLength, BUFF_OUTPUT_SIZE, len);
		len = BUFF_OUTPUT_SIZE - outputBufferLength;
	}
	memcpy(&outputBuffer[outputBufferLength], msg, len);
	outputBufferLength += len;
	outputBuffer[outputBufferLength] = 0;
	// printf("tx_buffer: %s", outputBuffer);
	return len;
}

/// Thread entry point.
void __cheri_compartment("uart") uart_entry()
{
	Debug::log("Configuring pinmux");
	auto pinSinks = MMIO_CAPABILITY(SonataPinmux::PinSinks, pinmux_pins_sinks);
	pinSinks->get(SonataPinmux::PinSink::pmod0_2).select(4); // uart1 tx -> pmod0_2
	auto blockSinks = MMIO_CAPABILITY(SonataPinmux::BlockSinks, pinmux_block_sinks);
	blockSinks->get(SonataPinmux::BlockSink::uart_1_rx).select(5); // pmod0_3 -> uart1 rx

	Debug::log("Configure UART");
	auto uart1 = MMIO_CAPABILITY(OpenTitanUart, uart1);
	uart1->init(115200);

	Debug::log("Initialise modem");
	tasks_set_initialise_modem();
	Debug::log("Begin the processing here");
	tasks_process();
	while(true)
    {
		// Is there anything to read.
		if(uart1->status & OpenTitanUart::StatusReceiveFull) {
			Debug::log("Rx buffer full!");
		}
		if(uart1->can_read()) {
			char c = uart1->readData;
			inputBuffer[inputBufferOffset] = c;
			inputBufferOffset++;
			inputBuffer[inputBufferOffset] = 0;	// Ensure termination
			// if((inputBufferOffset >= BUFF_INPUT_SIZE) || (c == '\r') || (c == '\n')) {
			if((inputBufferOffset >= BUFF_INPUT_SIZE) || (c == '\n')) {
				// printf("Read: %s", inputBuffer);
				process_serial_replies(inputBuffer, inputBufferOffset);
				inputBufferOffset = 0;
				inputBuffer[inputBufferOffset] = 0;	// Empty the string.
			}
		}
		if(outputBufferLength > 0) {
			// Debug::log("1 outputBufferLength = {}", outputBufferLength);
			if((outputBufferOffset >= outputBufferLength) || (outputBufferOffset > BUFF_OUTPUT_SIZE)) { // Sanity check
				Debug::log("ERROR Tx Offset exceeded Length! Impossible?");
				Debug::log("outputBufferLength = {}", outputBufferLength);
				Debug::log("outputBufferOffset = {}", outputBufferOffset);
				outputBufferLength = 0;
				outputBufferOffset = 0;
			} else if(uart1->can_write()) {
				// Debug::log("2 outputBufferOffset = {}", outputBufferOffset);
				uart1->writeData = outputBuffer[outputBufferOffset];
				outputBufferOffset++;
				if((outputBufferOffset >= outputBufferLength) || (outputBufferOffset > BUFF_OUTPUT_SIZE)) { // Sanity check
					outputBufferLength = 0;
					outputBufferOffset = 0;
				}
			}
		}

	}
}
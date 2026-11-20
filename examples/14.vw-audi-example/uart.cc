// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "compartment-macros.h"
#include "platform/sunburst/platform-pinmux.hh"
#include <array>
#include <compartment.h>
#include <cstdint>
#include <cstdlib>
#include <debug.hh>
#include <futex.h>
#include <interrupt.h>
#include <tick_macros.h>
#include <timeout.hh>
#include <platform/sunburst/platform-uart.hh>
#include <multiwaiter.h>
#include <queue.h>
#include "modem.hh"
#include "uart2.hh"


/*
* Await some (hardware-specified) number of bytes in the UART's RX FIFO before
* waking the core.  Set to Level1 to echo byte by byte.
*/
constexpr auto RXFifoLevel = OpenTitanUart::ReceiveWatermark::Level1;

/// Expose debugging features unconditionally for this compartment.
using Debug = ConditionalDebug<true, "uart">;

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(uart1InterruptCap, Uart1Interrupt, true, true);

#define BUFF_OUTPUT_SIZE	(200)
#define BUFF_INPUT_SIZE		(40)

char outputBuffer[BUFF_OUTPUT_SIZE + 1];
volatile uint16_t outputBufferOffset = 0;
volatile uint16_t outputBufferLength = 0;

char inputBuffer[BUFF_INPUT_SIZE + 1];
uint16_t inputBufferOffset = 0;

// The queue that we will wait on.
CHERI_SEALED(MessageQueue *) queue;

/**
 * Set the queue that the thread in this compartment will
 * use.
 */
// int __cheriot_compartment("uart") set_queue(CHERI_SEALED(MessageQueue *) newQueue)
int set_queue(CHERI_SEALED(MessageQueue *) newQueue)
{
	// set_queue#begin
	// Check that this is a valid queue
	size_t items;
	if (queue_items_remaining_sealed(newQueue, &items) != 0)
	{
		return -1;
	}
	// Set it in the global and allow the thread to start.
	queue = newQueue;
	Debug::log("Queue set to {}", queue);
	futex_wake(reinterpret_cast<uint32_t *>(&queue), 1);
	// set_queue#end
	return 0;
}

uint16_t append_to_tx_buffer(char* msg, uint16_t len)
{
	if((outputBufferLength + len) > BUFF_OUTPUT_SIZE) {	// It's too big - we must limit instead.
		Debug::log("Tx buffer truncating! Current {} of {}. Attempting to write {}.", outputBufferLength, BUFF_OUTPUT_SIZE, len);
		len = BUFF_OUTPUT_SIZE - outputBufferLength;
	}
	memcpy(&outputBuffer[outputBufferLength], msg, len);
	outputBufferLength += len;
	outputBuffer[outputBufferLength] = 0;
	printf("%s(): tx_buffer: %s\r\n", __FUNCTION__, outputBuffer);
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

	uart1->receive_watermark(RXFifoLevel);
    uart1->interrupt_enable(OpenTitanUart::InterruptReceiveWatermark);

	auto uart1InterruptFutex = interrupt_futex_get(STATIC_SEALED_VALUE(uart1InterruptCap));

	Debug::log("Create the multiwaiter");
	MultiWaiter multiwaiter;
	blocking_forever<multiwaiter_create>(MALLOC_CAPABILITY, &multiwaiter, 2);	// Space for 2 events.

	// Wait for the other thread to start!
	Debug::log("Wait for producer thread to start.");
	// Use the queue pointer as a futex.  It is initialised to
	// 0, if the other thread has stored a valid pointer here
	// then it will not be zero and so futex_wait will return
	// immediately.
	futex_wait(reinterpret_cast<uint32_t *>(&queue), 0);

	Debug::log("Initialise modem");
	tasks_set_initialise_modem();
	Debug::log("Begin the processing here");
	tasks_process();
	Debug::log("After tasks_process();");
	auto irqCount = *uart1InterruptFutex;
	do
    {
		std::array<EventWaiterSource, 2> events;
		events[0].eventSource = reinterpret_cast<void *>(const_cast<uint32_t*>(uart1InterruptFutex));
		events[0].value = irqCount;
		multiwaiter_queue_receive_init_sealed(&events[1], queue);

		do {
			irqCount = *uart1InterruptFutex;
			if(uart1->status & OpenTitanUart::StatusReceiveFull) {
				Debug::log("Rx buffer full!");
			}
			// Is there anything to read?
			while((uart1->status & OpenTitanUart::StatusReceiveEmpty) == 0) {
				char c = uart1->readData;
				inputBuffer[inputBufferOffset] = c;
				inputBufferOffset++;
				inputBuffer[inputBufferOffset] = 0;	// Ensure termination
				// if((inputBufferOffset >= BUFF_INPUT_SIZE) || (c == '\r') || (c == '\n')) {
				if((inputBufferOffset >= BUFF_INPUT_SIZE) || (c == '\n')) {
					// printf("Read: %s", inputBuffer);
					bool reply = process_serial_replies(inputBuffer, inputBufferOffset);
					inputBufferOffset = 0;
					inputBuffer[inputBufferOffset] = 0;	// Empty the string.
					if(reply) {
						// printf("%s(%i): outputBufferLength = %i, outputBufferOffset = %i\r\n", __FUNCTION__, __LINE__, outputBufferLength, outputBufferOffset);
						// printf("%s(%i): outputBuffer = %s\r\n", __FUNCTION__, __LINE__, outputBuffer);
						// Force the transmission here - it's the only way it works & I can't work out why the tx code below this doesn't do it for us.
						while(outputBufferLength > 0){
							while((uart1->status & OpenTitanUart::StatusTransmitFull) == 0) {
								uart1->writeData = outputBuffer[outputBufferOffset];
								outputBufferOffset += 1;
								if((outputBufferOffset >= outputBufferLength) || (outputBufferOffset > BUFF_OUTPUT_SIZE)) { // Sanity check
									outputBufferLength = 0;
									outputBufferOffset = 0;
									outputBuffer[0] = 0;
									break;
								}
							}
						}
						// break;
					}
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
					outputBuffer[0] = 0;
				} else {
					while((uart1->status & OpenTitanUart::StatusTransmitFull) == 0) {
						uart1->writeData = outputBuffer[outputBufferOffset];
						outputBufferOffset += 1;
						if((outputBufferOffset >= outputBufferLength) || (outputBufferOffset > BUFF_OUTPUT_SIZE)) { // Sanity check
							outputBufferLength = 0;
							outputBufferOffset = 0;
							outputBuffer[0] = 0;
							break;
						}
					}
				}
			}

			interrupt_complete(STATIC_SEALED_VALUE(uart1InterruptCap));
		} while(irqCount != *uart1InterruptFutex);

		int ret = blocking_forever<multiwaiter_wait>(multiwaiter, events.data(), events.size());
		Debug::Assert(ret == 0, "Multiwaiter failed: {}", ret);

		// Has there been a message for us to read?
		QueueMessage val = {0,0};
		if(events[1].value) {
			ret = non_blocking<queue_receive_sealed>(queue, &val);
			Debug::Assert(ret == 0, "Failed to receive message from queue: {}", ret);
			// Debug::log("Received message type {}, data {}", val.messType, val.messData);
			char msg[100];
			snprintf(msg, 99, "t=%i&v=%i", val.messType, val.messData);
			// printf("%s %u: msg: %s\n", __FILE__, __LINE__, msg);
			tasks_send_message(msg);
			tasks_process();	// Begin the processing now.
			// printf("%s(%u): Off = %i, len = %i: %s", __FUNCTION__, __LINE__, outputBufferOffset, outputBufferLength, outputBuffer);
			// Is there anything new to write to the serial?
			if((outputBufferLength > 0) && (outputBufferOffset < outputBufferLength)) {
				printf("%s(%u): Off = %i, len = %i: %s",__FUNCTION__, __LINE__, outputBufferOffset, outputBufferLength, outputBuffer);
				while((uart1->status & OpenTitanUart::StatusTransmitFull) == 0) {
					// Debug::log("2 outputBufferOffset[{}/{}] = {}", outputBufferOffset, outputBufferLength, outputBuffer[outputBufferOffset]);
					uart1->writeData = outputBuffer[outputBufferOffset];
					outputBufferOffset += 1;
					if((outputBufferOffset >= outputBufferLength) || (outputBufferOffset > BUFF_OUTPUT_SIZE)) { // Sanity check
						outputBufferLength = 0;
						outputBufferOffset = 0;
						outputBuffer[0] = 0;
						break;
					}
				}
			}
		}
	} while(true);
	// } while((irqCount != *uart1InterruptFutex) || (futex_wait(uart1InterruptFutex, irqCount) == 0));
	Debug::log("ERROR! You've exited the main loop! This should not be possible!");
}

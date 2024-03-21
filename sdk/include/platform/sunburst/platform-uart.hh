#pragma once
#pragma push_macro("CHERIOT_PLATFORM_CUSTOM_UART")
#define CHERIOT_PLATFORM_CUSTOM_UART
#include_next <platform-uart.hh>
#pragma pop_macro("CHERIOT_PLATFORM_CUSTOM_UART")

/**
 * OpenTitan UART
 *
 * This peripheral's source and documentation can be found at:
 * https://github.com/lowRISC/opentitan/tree/ab878b5d3578939a04db72d4ed966a56a869b2ed/hw/ip/uart
 *
 * Rendered register documentation is served at:
 * https://opentitan.org/book/hw/ip/uart/doc/registers.html
 */
template<unsigned DefaultBaudRate = 115'200>
class OpenTitanUart
{
	public:
	/**
	 * Interrupt State Register.
	 */
	uint32_t intrState;
	/**
	 * Interrupt Enable Register.
	 */
	uint32_t intrEnable;
	/**
	 * Interrupt Test Register.
	 */
	uint32_t intrTest;
	/**
	 * Alert Test Register (unused).
	 */
	uint32_t alertTest;
	/**
	 * Control Register.
	 */
	uint32_t ctrl;
	/**
	 * Status Register.
	 */
	uint32_t status;
	/**
	 * UART Read Data.
	 */
	uint32_t rData;
	/**
	 * UART Write Data.
	 */
	uint32_t wData;
	/**
	 * UART FIFO Control Register.
	 */
	uint32_t fifoCtrl;
	/**
	 * UART FIFO Status Register.
	 */
	uint32_t fifoStatus;
	/**
	 * TX Pin Override Control.
	 *
	 * Gives direct SW control over TX pin state.
	 */
	uint32_t ovrd;
	/**
	 * UART Oversampled Values.
	 */
	uint32_t val;
	/**
	 * UART RX Timeout Control.
	 */
	uint32_t timeoutCtrl;

	void init(unsigned baudRate = DefaultBaudRate) volatile
	{
		// NCO = 2^20 * baud rate / cpu frequency
		const uint32_t NCO =
		  ((static_cast<uint64_t>(baudRate) << 20) / CPU_TIMER_HZ);
		// Set the baud rate and enable transmit & receive
		ctrl = (NCO << 16) | 0b11;
	};

	bool can_write() volatile
	{
		return (fifoStatus & 0xff) < 32;
	};

	bool can_read() volatile
	{
		return ((fifoStatus >> 16) & 0xff) > 0;
	};

	/**
	 * Write one byte, blocking until the byte is written.
	 */
	void blocking_write(uint8_t byte) volatile
	{
		while (!can_write()) {}
		wData = byte;
	}

	/**
	 * Read one byte, blocking until a byte is available.
	 */
	uint8_t blocking_read() volatile
	{
		while (!can_read()) {}
		return rData;
	}
};

#ifndef CHERIOT_PLATFORM_CUSTOM_UART
using Uart = OpenTitanUart<>;
static_assert(IsUart<Uart>);
#endif

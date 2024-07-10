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
struct OpenTitanUart
{
	/**
	 * Interrupt State Register.
	 */
	uint32_t interruptState;
	/**
	 * Interrupt Enable Register.
	 */
	uint32_t interruptEnable;
	/**
	 * Interrupt Test Register.
	 */
	uint32_t interruptTest;
	/**
	 * Alert Test Register (unused).
	 */
	uint32_t alertTest;
	/**
	 * Control Register.
	 */
	uint32_t control;
	/**
	 * Status Register.
	 */
	uint32_t status;
	/**
	 * UART Read Data.
	 */
	uint32_t readData;
	/**
	 * UART Write Data.
	 */
	uint32_t writeData;
	/**
	 * UART FIFO Control Register.
	 */
	uint32_t fifoCtrl;
	/**
	 * UART FIFO Status Register.
	 */
	uint32_t fifoStatus;
	/**
	 * Transmit Pin Override Control.
	 *
	 * Gives direct software control over the transmit pin state.
	 */
	uint32_t override;
	/**
	 * UART Oversampled Values.
	 */
	uint32_t values;
	/**
	 * UART Receive Timeout Control.
	 */
	uint32_t timeoutControl;

	/// Control Register Fields
	enum : uint32_t
	{
		/// Sets the BAUD clock rate from the numerically controlled oscillator.
		ControlNco = 0xff << 16,
		/// Set the number of character times the line must be low
		/// which will be interpreted as a break.
		ControlReceiveBreakLevel = 0b11 << 8,
		/// When set, odd parity is used, otherwise even parity is used.
		ControlParityOdd = 1 << 7,
		/// Enable party on both transmit and receive lines.
		ControlParityEnable = 1 << 6,
		/// When set, incoming received bits are forwarded to the transmit line.
		ControlLineLoopback = 1 << 5,
		/// When set, outgoing transmitted bits are routed back the receiving
		/// line.
		ControlSystemLoopback = 1 << 4,
		/// Enable the noise filter on the receiving line.
		ControlNoiseFilter = 1 << 2,
		/// Enable receiving bits.
		ControlReceiveEnable = 1 << 1,
		/// Enable transmitting bits.
		ControlTransmitEnable = 1 << 0,
	};

	void init(unsigned baudRate = 115'200) volatile
	{
		// Nco = 2^20 * baud rate / cpu frequency
		const uint32_t Nco =
		  ((static_cast<uint64_t>(baudRate) << 20) / CPU_TIMER_HZ);
		// Set the baud rate and enable transmit & receive
		control = (Nco << 16) | ControlTransmitEnable | ControlReceiveEnable;
	}

	[[gnu::always_inline]] uint16_t transmit_fifo_level() volatile
	{
		return fifoStatus & 0xff;
	}

	[[gnu::always_inline]] uint16_t receive_fifo_level() volatile
	{
		return ((fifoStatus >> 16) & 0xff);
	}

	bool can_write() volatile
	{
		return transmit_fifo_level() < 32;
	}

	bool can_read() volatile
	{
		return receive_fifo_level() > 0;
	}

	/**
	 * Write one byte, blocking until the byte is written.
	 */
	void blocking_write(uint8_t byte) volatile
	{
		while (!can_write()) {}
		writeData = byte;
	}

	/**
	 * Read one byte, blocking until a byte is available.
	 */
	uint8_t blocking_read() volatile
	{
		while (!can_read()) {}
		return readData;
	}
};

#ifndef CHERIOT_PLATFORM_CUSTOM_UART
using Uart = OpenTitanUart;
static_assert(IsUart<Uart>);
#endif

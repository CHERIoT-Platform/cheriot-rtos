// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <platform/concepts/uart.hh>
#include <stdint.h>

/**
 * Generic 16550A memory-mapped register layout.
 *
 * The registers are 8 bits wide, but typically the bus supports only 4-byte
 * (or larger) transactions and so they are padded to a 32-bit word.  The
 * template parameter allows this to be controlled.
 */
template<typename RegisterType = uint32_t>
class Uart16550
{
	static void no_custom_init() {}

	public:
	/**
	 * The interface to the read/write FIFOs for this UART.
	 *
	 * This is also the low byte of the divisor when the divisor latch (bit 7 of
	 * the `lineControl`) is set.
	 */
	RegisterType data;
	/**
	 * Interrupt-enabled control / status.  Write 1 to enabled, 0 to disable, to
	 * each of the low four bits:
	 *
	 * 0: Data-receive interrupt
	 * 1: Transmit holding register empty interrupt
	 * 2: Receive line status interrupts
	 * 3: Modem status interrupts.
	 *
	 * When bit 7 of `lineControl` is set, this is instead the
	 * divisor-latch-high register and stores the high 8 bits of the divisor.
	 */
	RegisterType intrEnable;
	/**
	 * Interrupt identification and FIFO enable/disable.
	 *
	 * We only care about the low bit here, which enables the FIFO.
	 */
	RegisterType intrIDandFifo;
	/**
	 * Specifies properties of the line (stop bit, parity, and so on).  The
	 * only bit that we use is bit 7, the divisor latch, which enables writing
	 * the speed.
	 */
	RegisterType lineControl;
	/**
	 * Modem control.
	 */
	RegisterType modemControl;
	/**
	 * The line status word.  The bits that we care about are:
	 *
	 * 0: Receive ready
	 * 5: Transmit buffer empty
	 */
	const RegisterType LineStatus;
	/**
	 * Modem status.
	 */
	const RegisterType ModemStatus;
	/**
	 * Scratch register.  Unused.
	 */
	RegisterType scratch;

	/**
	 * Returns true if the transmit buffer is empty.
	 */
	__always_inline bool can_write() volatile
	{
#ifdef SIMULATION
		// If we're in a simulator, we're running *much* slower than the thing
		// handling the UART, so assume that you can always write to the UART.
		return true;
#else
		return LineStatus & (1 << 5);
#endif
	}

	/**
	 * Returns true if the receive buffer is not.
	 */
	__always_inline bool can_read() volatile
	{
		return LineStatus & (1 << 0);
	}

	/**
	 * Read one byte, blocking until a byte is available.
	 */
	uint8_t blocking_read() volatile
	{
		while (!can_read()) {}
		return data;
	}

	/**
	 * Write one byte, blocking until the byte is written.
	 */
	void blocking_write(uint8_t byte) volatile
	{
		while (!can_write()) {}
		data = byte;
	}

	/**
	 * Initialise the UART.
	 */
	template<typename T = decltype(no_custom_init)>
	void init(int divisor = 1, T &&otherSetup = no_custom_init) volatile
	{
		// Disable interrupts
		intrEnable = 0x00;
		// Set the divisor latch (we're going to write the divisor) and set the
		// character width to 8 bits, one stop bit, no parity.
		lineControl = 0x83;
		// Set the divisor
		data       = divisor & 0xff;
		intrEnable = (divisor >> 8) & 0xff;
		// Run any other setup that we were asked to do.
		otherSetup();
		// Clear the divisor latch
		lineControl = 0x03;
		// Enable the FIFO and reset
		// Enabled bits:
		// 0 - Enable FIFOs
		// 1 - Clear receive FIFO
		// 2 - Clear send FIFO
		// 5 - 64-byte FIFO (if available)
		intrIDandFifo = 0x1;
	}
};

// A platform can provide a custom version of this.
#ifndef CHERIOT_PLATFORM_CUSTOM_UART
/// The default UART type.
using Uart = Uart16550<uint32_t>;
// Check that our UART matches the concept.
static_assert(IsUart<Uart>);
#endif

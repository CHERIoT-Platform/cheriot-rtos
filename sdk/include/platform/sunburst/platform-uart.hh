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

	/// OpenTitan UART Interrupts
	typedef enum [[clang::flag_enum]]
	: uint32_t{
	    /// Raised if the transmit FIFO is empty.
	    InterruptTransmitEmpty = 1 << 8,
	    /// Raised if the receiver has detected a parity error.
	    InterruptReceiveParityErr = 1 << 7,
	    /// Raised if the receive FIFO has characters remaining in the FIFO
	    /// without being
	    /// retreived for the programmed time period.
	    InterruptReceiveTimeout = 1 << 6,
	    /// Raised if break condition has been detected on receive.
	    InterruptReceiveBreakErr = 1 << 5,
	    /// Raised if a framing error has been detected on receive.
	    InterruptReceiveFrameErr = 1 << 4,
	    /// Raised if the receive FIFO has overflowed.
	    InterruptReceiveOverflow = 1 << 3,
	    /// Raised if the transmit FIFO has emptied and no transmit is ongoing.
	    InterruptTransmitDone = 1 << 2,
	    /// Raised if the receive FIFO is past the high-water mark.
	    InterruptReceiveWatermark = 1 << 1,
	    /// Raised if the transmit FIFO is past the high-water mark.
	    InterruptTransmitWatermark = 1 << 0,
	  } OpenTitanUartInterrupt;

	/// FIFO Control Register Fields
	enum [[clang::flag_enum]] : uint32_t{
	  /// Reset the transmit FIFO.
	  FifoControlTransmitReset = 1 << 1,
	  /// Reset the receive FIFO.
	  FifoControlReceiveReset = 1 << 0,
	};

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

	/// The encoding for different transmit watermark levels.
	enum class TransmitWatermark
	{
		Level1  = 0x0,
		Level2  = 0x1,
		Level4  = 0x2,
		Level8  = 0x3,
		Level16 = 0x4,
	};

	/// The encoding for different receive watermark levels.
	enum class ReceiveWatermark
	{
		Level1  = 0x0,
		Level2  = 0x1,
		Level4  = 0x2,
		Level8  = 0x3,
		Level16 = 0x4,
		Level32 = 0x5,
		Level64 = 0x6,
	};

	/**
	 * Configure parity.
	 *
	 * When `enableParity` is set, parity will be enabled.
	 * When `oddParity` is set, the odd parity will be used.
	 */
	void parity(bool enableParity = true, bool oddParity = false) volatile
	{
		control = (control & ~(ControlParityEnable | ControlParityOdd)) |
		          (enableParity ? ControlParityEnable : 0) |
		          (oddParity ? ControlParityOdd : 0);
	}

	/**
	 * Configure loopback.
	 *
	 * When `systemLoopback` is set, outgoing transmitted bits are routed back
	 * the receiving line. When `lineLoopback` is set, incoming received bits
	 * are forwarded to the transmit line.
	 */
	void loopback(bool systemLoopback = true,
	              bool lineLoopback   = false) volatile
	{
		control = (control & ~(ControlSystemLoopback | ControlLineLoopback)) |
		          (systemLoopback ? ControlSystemLoopback : 0) |
		          (lineLoopback ? ControlLineLoopback : 0);
	}

	/// Clears the contents of the receive and transmit FIFOs.
	void fifos_clear() volatile
	{
		fifoCtrl = (fifoCtrl & ~0b11) | FifoControlTransmitReset |
		           FifoControlReceiveReset;
	}

	/**
	 * Sets the level transmit watermark.
	 *
	 * When the number of bytes in the transmit FIFO reach this level,
	 * the transmit watermark interrupt will fire.
	 */
	void transmit_watermark(TransmitWatermark level) volatile
	{
		fifoCtrl = static_cast<uint32_t>(level) << 5 | (fifoCtrl & 0x1f);
	}

	/**
	 * Sets the level receive watermark.
	 *
	 * When the number of bytes in the receive FIFO reach this level,
	 * the receive watermark interrupt will fire.
	 */
	void receive_watermark(ReceiveWatermark level) volatile
	{
		fifoCtrl = static_cast<uint32_t>(level) << 5 | (fifoCtrl & 0b11100011);
	}

	/// Enable the given interrupt.
	void interrupt_enable(OpenTitanUartInterrupt interrupt) volatile
	{
		interruptEnable = interruptEnable | interrupt;
	}

	/// Disable the given interrupt.
	void interrupt_disable(OpenTitanUartInterrupt interrupt) volatile
	{
		interruptEnable = interruptEnable & ~interrupt;
	}

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

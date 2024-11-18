#pragma once
#include <cdefs.h>
#include <debug.hh>
#include <stdint.h>

/**
 * A Simple Driver for the Sonata's SPI.
 *
 * Documentation source can be found at:
 * https://github.com/lowRISC/sonata-system/blob/1a59633d2515d4fe186a07d53e49ff95c18d9bbf/doc/ip/spi.md
 *
 * Rendered documentation is served from:
 * https://lowrisc.org/sonata-system/doc/ip/spi.html
 */
struct SonataSpi
{
	/**
	 * The Sonata SPI block doesn't currently have support for interrupts.
	 * The following registers are reserved for future use.
	 */
	uint32_t interruptState;
	uint32_t interruptEnable;
	uint32_t interruptTest;
	/**
	 * Configuration register. Controls how the SPI block transmits and
	 * receives data. This register can be modified only whilst the SPI block
	 * is idle.
	 */
	uint32_t configuration;
	/**
	 * Controls the operation of the SPI block. This register can
	 * be modified only whilst the SPI block is idle.
	 */
	uint32_t control;
	/// Status information about the SPI block
	uint32_t status;
	/**
	 * Writes to this begin an SPI operation.
	 * Writes are ignored when the SPI block is active.
	 */
	uint32_t start;
	/**
	 * Data from the receive FIFO. When read the data is popped from the FIFO.
	 * If the FIFO is empty data read is undefined.
	 */
	uint32_t receiveFifo;
	/**
	 * Bytes written here are pushed to the transmit FIFO. If the FIFO is full
	 * writes are ignored.
	 */
	uint32_t transmitFifo;

	/// Configuration Register Fields
	enum : uint32_t
	{
		/**
		 * The length of a half period (i.e. positive edge to negative edge) of
		 * the SPI clock, measured in system clock cycles reduced by 1. For
		 * example, at a 50 MHz system clock, a value of 0 gives a 25 MHz SPI
		 * clock, a value of 1 gives a 12.5 MHz SPI clock, a value of 2 gives
		 * a 8.33 MHz SPI clock and so on.
		 */
		ConfigurationHalfClockPeriodMask = 0xffu << 0,
		/*
		 * When set the most significant bit (MSB) is the first bit sent and
		 * received with each byte
		 */
		ConfigurationMSBFirst = 1u << 29,
		/*
		 * The phase of the spi_clk signal. when clockphase is 0, data is
		 * sampled on the leading edge and changes on the trailing edge. The
		 * first data bit is immediately available before the first leading edge
		 * of the clock when transmission begins. When clockphase is 1, data is
		 * sampled on the trailing edge and change on the leading edge.
		 */
		ConfigurationClockPhase = 1u << 30,
		/*
		 * The polarity of the spi_clk signal. When ClockPolarity is 0, clock is
		 * low when idle and the leading edge is positive. When ClkPolarity is
		 * 1, clock is high when idle and the leading edge is negative
		 */
		ConfigurationClockPolarity = 1u << 31,
	};

	/// Control Register Fields
	enum : uint32_t
	{
		/// Write 1 to clear the transmit FIFO.
		ControlTransmitClear = 1 << 0,
		/// Write 1 to clear the receive FIFO.
		ControlReceiveClear = 1 << 1,
		/**
		 * When set bytes from the transmit FIFO are sent. When clear the state
		 * of the outgoing spi_cipo is undefined whilst the SPI clock is
		 * running.
		 */
		ControlTransmitEnable = 1 << 2,
		/**
		 * When set incoming bits are written to the receive FIFO. When clear
		 * incoming bits are ignored.
		 */
		ControlReceiveEnable = 1 << 3,
		/**
		 * The watermark level for the transmit FIFO, depending on the value
		 * the interrupt will trigger at different points
		 */
		ControlTransmitWatermarkMask = 0xf << 4,
		/**
		 * The watermark level for the receive FIFO, depending on the value the
		 * interrupt will trigger at different points
		 */
		ControlReceiveWatermarkMask = 0xf << 8,
	};

	/// Status Register Fields
	enum : uint32_t
	{
		/// Number of items in the transmit FIFO.
		StatusTxFifoLevel = 0xffu << 0,
		/// Number of items in the receive FIFO.
		StatusRxFifoLevel = 0xffu << 8,
		/**
		 * When set the transmit FIFO is full and any data written to it will
		 * be ignored.
		 */
		StatusTxFifoFull = 1u << 16,
		/**
		 * When set the receive FIFO is empty and any data read from it will be
		 * undefined.
		 */
		StatusRxFifoEmpty = 1u << 17,
		/// When set the SPI block is idle and can accept a new start command.
		StatusIdle = 1u << 18,
	};

	/// Start Register Fields
	enum : uint32_t
	{
		/// Number of bytes to receive/transmit in the SPI operation
		StartByteCountMask = 0x7ffu,
	};

	/// Flag set when we're debugging this driver.
	static constexpr bool DebugSonataSpi = false;

	/// Helper for conditional debug logs and assertions.
	using Debug = ConditionalDebug<DebugSonataSpi, "Sonata SPI">;

	/**
	 * Initialises the SPI block
	 *
	 * @param ClockPolarity When false, the clock is low when idle and the
	 *        leading edge is positive. When true, the opposite behaviour is
	 *        set.
	 * @param ClockPhase When false, data is sampled on the leading edge and
	 *        changes on the trailing edge. When true, the opposite behaviour is
	 *        set.
	 * @param MsbFirst When true, the first bit of each byte sent is the most
	 *        significant bit, as oppose to the least significant bit.
	 * @param HalfClockPeriod The length of a half period of the SPI clock,
	 *        measured in system clock cycles reduced by 1.
	 */
	void init(const bool     ClockPolarity,
	          const bool     ClockPhase,
	          const bool     MsbFirst,
	          const uint16_t HalfClockPeriod) volatile
	{
		configuration = (ClockPolarity ? ConfigurationClockPolarity : 0) |
		                (ClockPhase ? ConfigurationClockPhase : 0) |
		                (MsbFirst ? ConfigurationMSBFirst : 0) |
		                (HalfClockPeriod & ConfigurationHalfClockPeriodMask);
	}

	/// Waits for the SPI device to become idle
	void wait_idle() volatile
	{
		// Wait whilst IDLE field in STATUS is low
		while ((status & StatusIdle) == 0) {}
	}

	/**
	 * Sends `len` bytes from the given `data` buffer,
	 * where `len` is at most `0x7ff`.
	 */
	void blocking_write(const uint8_t data[], uint16_t len) volatile
	{
		Debug::Assert(len <= 0x7ff,
		              "You can't transfer more than 0x7ff bytes at a time.");
		len &= StartByteCountMask;

		wait_idle();
		control = ControlTransmitEnable;
		start   = len;

		uint32_t transmitAvailable = 0;
		for (uint32_t i = 0; i < len; ++i)
		{
			if (transmitAvailable == 0)
			{
				while (transmitAvailable < 64)
				{
					// Read number of bytes in TX FIFO to calculate space
					// available for more bytes
					transmitAvailable = 64 - (status & StatusTxFifoLevel);
				}
			}
			transmitFifo = data[i];
			transmitAvailable--;
		}
	}

	/*
	 * Receives `len` bytes and puts them in the `data` buffer,
	 * where `len` is at most `0x7ff`.
	 *
	 * This method will block until the requested number of bytes
	 * has been seen. There is currently no timeout.
	 */
	void blocking_read(uint8_t data[], uint16_t len) volatile
	{
		Debug::Assert(len <= 0x7ff,
		              "You can't receive more than 0x7ff bytes at a time.");
		len &= StartByteCountMask;
		wait_idle();
		control = ControlReceiveEnable;
		start   = len;

		for (uint32_t i = 0; i < len; ++i)
		{
			// Wait for at least one byte to be available in the RX FIFO
			while ((status & StatusRxFifoLevel) == 0) {}
			data[i] = static_cast<uint8_t>(receiveFifo);
		}
	}
};

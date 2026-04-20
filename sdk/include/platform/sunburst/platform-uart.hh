#pragma once

#ifndef DEFAULT_UART_BAUD_RATE
#	define DEFAULT_UART_BAUD_RATE 921'600
#endif

#include <bitpacks.hh>
#include <platform/concepts/uart.hh>
#include <utils.hh>

/**
 * OpenTitan UART
 *
 * This peripheral's source and documentation can be found at:
 * https://github.com/lowRISC/opentitan/tree/ab878b5d3578939a04db72d4ed966a56a869b2ed/hw/ip/uart
 *
 * Rendered register documentation is served at:
 * https://opentitan.org/book/hw/ip/uart/doc/registers.html
 */
struct OpenTitanUart : private utils::NoCopyNoMove
{
	struct Interrupts : Bitpack<uint32_t>
	{
		BITPACK_USUAL_PREFIX;
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(TransmitWatermark, 0);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveWatermark, 1);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(TransmitDone, 2);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveOverflowError, 3);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveFrameError, 4);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveBreakError, 5);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveTimeout, 6);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveParityError, 7);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(TransmitEmpty, 8);
	};

	struct InterruptState : BitpackDerived<Interrupts>
	{
		BITPACK_DERIVED_PREFIX;

		// TransmitWatermark is a status, not an event, so RO, not RW1C.
		BITPACK_DERIVED_FIELD_CONST_FOR_TYPE(TransmitWatermark, true);

		// ReceiveWatermark is a status, not an event, so RO, not RW1C.
		BITPACK_DERIVED_FIELD_CONST_FOR_TYPE(ReceiveWatermark, true);

		// TransmitEmpty is a status, not an event, so RO, not RW1C.
		BITPACK_DERIVED_FIELD_CONST_FOR_TYPE(TransmitEmpty, true);
	};

	InterruptState interruptState;
	Interrupts     interruptEnable;
	Interrupts     interruptTest;

	/**
	 * Alert Test Register (unused).
	 */
	uint32_t alertTest;

	struct Control : Bitpack<uint32_t>
	{
		BITPACK_USUAL_PREFIX;
		BITPACK_DEFINE_TYPE_ENUM_BOOL_DISABLED_ENABLED(Transmit, 0);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_DISABLED_ENABLED(Receive, 1);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_DISABLED_ENABLED(NoiseFilter, 2);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_DISABLED_ENABLED(SystemLoopback, 3);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_DISABLED_ENABLED(LineLoopback, 4);
		BITPACK_DEFINE_TYPE_ENUM_CLASS(Parity, uint8_t, 5, 6) {
			None = 0b00,
			Even = 0b01,
			Odd  = 0b11,
		};

		BITPACK_DEFINE_TYPE_NUMERIC(BreakLevel, uint8_t, 8, 9);
		BITPACK_DEFINE_TYPE_NUMERIC(Nco, uint16_t, 16, 31);
	} control;

	const struct Status : Bitpack<uint32_t>
	{
		BITPACK_USUAL_PREFIX;
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(TransmitFull, 0);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveFull, 1);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(TransmitEmpty, 2);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(TransmitIdle, 3);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveIdle, 4);
		BITPACK_DEFINE_TYPE_ENUM_BOOL_CLEARED_ASSERTED(ReceiveEmpty, 5);
	} status; // NOLINT(readability-identifier-naming)

	/**
	 * UART Read Data.
	 */
	uint32_t readData;
	/**
	 * UART Write Data.
	 */
	uint32_t writeData;

	struct FIFOControl : Bitpack<uint32_t>
	{
		BITPACK_USUAL_PREFIX;

		BITPACK_DEFINE_TYPE_ENUM_BOOL(Receive, AsIs, Reset, 0);
		BITPACK_DEFINE_TYPE_ENUM_BOOL(Transmit, AsIs, Reset, 1);

		BITPACK_DEFINE_TYPE_ENUM_CLASS(TransmitWatermark, uint8_t, 2, 4) {
			Level1  = 0b000,
			Level2  = 0b001,
			Level4  = 0b010,
			Level8  = 0b011,
			Level16 = 0b100,
			Level32 = 0b101,
			Level64 = 0b110,
		};
		BITPACK_DEFINE_TYPE_ENUM_CLASS(ReceiveWatermark, uint8_t, 5, 7) {
			Level1  = 0b000,
			Level2  = 0b001,
			Level4  = 0b010,
			Level8  = 0b011,
			Level16 = 0b100,
		};
	} fifoControl;

	/**
	 * UART FIFO Status Register.
	 */
	const struct FIFOStatus : Bitpack<uint32_t>
	{
		BITPACK_USUAL_PREFIX;

		BITPACK_DEFINE_TYPE_NUMERIC(TransmitLevel, uint8_t, 0, 7);
		BITPACK_DEFINE_TYPE_NUMERIC(ReceiveLevel, uint8_t, 16, 23);
	} fifoStatus; // NOLINT(readability-identifier-naming)

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

	/**
	 * Configure parity.
	 *
	 * When `enableParity` is set, parity will be enabled.
	 * When `oddParity` is set, the odd parity will be used.
	 */
	void parity(bool enableParity = true, bool oddParity = false) volatile
	{
		using enum Control::Parity;
		control.set(enableParity ? (oddParity ? Odd : Even) : None);
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
		control.alter([=](auto v) {
			BITPACK_TQVAL_OP(v, =, SystemLoopback{systemLoopback});
			BITPACK_TQVAL_OP(v, =, LineLoopback{lineLoopback});
			return v;
		});
	}

	/// Clears the contents of the receive and transmit FIFOs.
	void fifos_clear() volatile
	{
		fifoControl.alter([](auto v) {
			BITPACK_QVAL_OP(v, =, Transmit::Reset);
			BITPACK_QVAL_OP(v, =, Receive::Reset);
			return v;
		});
	}

	/**
	 * Sets the level transmit watermark.
	 *
	 * When the number of bytes in the transmit FIFO reach this level,
	 * the transmit watermark interrupt will fire.
	 */
	void transmit_watermark(FIFOControl::TransmitWatermark level) volatile
	{
		fifoControl.set(level);
	}

	/**
	 * Sets the level receive watermark.
	 *
	 * When the number of bytes in the receive FIFO reach this level,
	 * the receive watermark interrupt will fire.
	 */
	void receive_watermark(FIFOControl::ReceiveWatermark level) volatile
	{
		fifoControl.set(level);
	}

	/// Enable the given interrupt by name
	template<typename Interrupt>
	void interrupt_enable() volatile
	{
		interruptEnable.view<Interrupt>() = Interrupt{true};
	}

	/// Disable the given interrupt by name
	template<typename Interrupt>
	void interrupt_disable() volatile
	{
		interruptEnable.view<Interrupt>() = Interrupt{false};
	}

	/// Enable the given interrupt(s) by value
	void interrupt_enable(Interrupts interrupt) volatile
	{
		interruptEnable.alter([interrupt](auto v) {
			return decltype(v){v.raw() | interrupt.raw()};
		});
	}

	/// Disable the given interrupt(s) by value
	void interrupt_disable(Interrupts interrupt) volatile
	{
		interruptEnable.alter([interrupt](auto v) {
			return decltype(v){v.raw() & ~interrupt.raw()};
		});
	}

	void init(unsigned baudRate = DEFAULT_UART_BAUD_RATE) volatile
	{
		// Nco = 2^20 * baud rate / cpu frequency
		const uint16_t Nco =
		  ((static_cast<uint64_t>(baudRate) << 20) / CPU_TIMER_HZ);

		control.alter([=](auto v) {
			BITPACK_TQVAL_OP(v, =, Nco{Nco});
			BITPACK_QVAL_OP(v, =, Transmit::Enabled);
			BITPACK_QVAL_OP(v, =, Receive::Enabled);
			return v;
		});
	}

	/// Turn off the transceivers
	void disable() volatile
	{
		control.alter([=](auto v) {
			BITPACK_QVAL_OP(v, =, Transmit::Disabled);
			BITPACK_QVAL_OP(v, =, Receive::Disabled);
			return v;
		});
	}

	/**
	 * Reset the control register to all zeros.  That disables the transceivers,
	 * clears any loopbacks, disables parity, and so on.
	 */
	void reset() volatile
	{
		control = decltype(control){0};
	}

	[[gnu::always_inline]] uint16_t transmit_fifo_level() volatile
	{
		return BITPACK_BY_QTYPE(fifoStatus.read(), TransmitLevel).raw();
	}

	[[gnu::always_inline]] uint16_t receive_fifo_level() volatile
	{
		return BITPACK_BY_QTYPE(fifoStatus.read(), ReceiveLevel).raw();
	}

	bool can_write() volatile
	{
		return BITPACK_QVAL_OP(status.read(), ==, TransmitFull::Cleared);
	}

	bool can_read() volatile
	{
		return BITPACK_QVAL_OP(status.read(), ==, ReceiveEmpty::Cleared);
	}

	/**
	 * Write one byte, blocking until the byte is written.
	 */
	void blocking_write(uint8_t byte) volatile
	{
		while (!can_write())
		{
		}
		writeData = byte;
	}

	/**
	 * Read one byte, blocking until a byte is available.
	 */
	uint8_t blocking_read() volatile
	{
		while (!can_read())
		{
		}
		return readData;
	}
};

#ifndef CHERIOT_PLATFORM_CUSTOM_UART
using Uart = OpenTitanUart;
static_assert(IsUart<Uart>);
#endif

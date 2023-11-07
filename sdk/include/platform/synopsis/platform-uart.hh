#pragma once
#pragma push_macro("CHERIOT_PLATFORM_CUSTOM_UART")
#define CHERIOT_PLATFORM_CUSTOM_UART
#include_next <platform-uart.hh>
#pragma pop_macro("CHERIOT_PLATFORM_CUSTOM_UART")

/**
 * The Synopsis extended 16550 UART has two additional registers that are not
 * part of a standard 16550, one of which allows for fractional multipliers.
 *
 * This is always instantiated with 32-bit device registers.  The default baud
 * rate is set as a template parameter and can be overridden in `init`.
 */
template<unsigned DefaultBaudRate = 115'200>
struct SynopsisExtended16550 : public Uart16550<uint32_t>
{
	/**
	 * Loopback register.  Unused.
	 */
	uint32_t loopback;

	/**
	 * Divisor latch fraction.  When the divisor latch is set (for writing the
	 * divisor), this is used for the fraction.  RS-232 requires a 3% tolerance
	 * for the baud rate and an integer divisor over the clock frequency of the
	 * host may not get within this range.  This register is used to provide a
	 * 4-bit correction that allows baud rates to stay within the required
	 * range.
	 */
	uint32_t divisorLatchFraction;

	/**
	 * The divisor is calculated from 16 times the baud rate.  The baud rate is
	 * in the range of KHz to hundreds of KHz, whereas the clock frequency is
	 * typically tens to hundreds of MHz.  The divisor must fit in a 16-bit
	 * integer (written across two 8-bit device registers) and so is scaled to
	 * ensure that it fits (by a power of two, which makes it trivial to
	 * reverse the multiplication in the hardware).
	 */
	static constexpr uint32_t BaudScaleFactorShift = 4;

	/**
	 * The value in `divisorLatchFraction` is truncated to this many bits.
	 */
	static constexpr uint32_t BaudFractionBits = 4;

	/**
	 * Initialise the UART.
	 */
	void init(unsigned baudRate       = DefaultBaudRate,
	          unsigned timerFrequency = CPU_TIMER_HZ) volatile
	{
		// The multiplier that we will scale the baud rate by.
		constexpr uint32_t Multiplier = (1 << BaudScaleFactorShift);
		// The scaled baud rate is used in all of the remaining calculations.
		// See `BaudScaleFactorShift` for an explanation.
		uint32_t scaledBaudRate = baudRate * Multiplier;
		// The divisor to be programmed into the device.
		uint32_t divisor = timerFrequency / scaledBaudRate;
		// The remainder of the divisor.
		uint32_t remainder = timerFrequency % scaledBaudRate;
		// The remainder is scaled relative to the baud rate to give a
		// correction factor.
		remainder = (remainder << (BaudScaleFactorShift + 1)) / scaledBaudRate;
		// The remainder is rounded up to give the value that can be programmed
		// into the device register.
		remainder = (remainder >> 1) + (remainder & 0x1);
		// Set up the base UART
		Uart16550<uint32_t>::init(divisor,
		                          [&]() { divisorLatchFraction = remainder; });
		// Write a character with the low bit set.  Any value is fine here, but
		// if we use 0x7 (ASCII bell character) then we should get a beep if it
		// goes through to the receiver and so can tell whether this worked.
		blocking_write(0x7);
	}
};

#ifndef CHERIOT_PLATFORM_CUSTOM_UART
using Uart = SynopsisExtended16550<>;
#endif

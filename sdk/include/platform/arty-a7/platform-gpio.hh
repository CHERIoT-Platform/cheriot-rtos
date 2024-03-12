#pragma once
#include <cdefs.h>
#include <stdint.h>

/**
 * The Arty A7 configuration has two LEDs (LD5 and LD6), four buttons, and four
 * switches, exposed over GPIO.
 *
 * This provides a simple driver for them.
 */
struct GPIO
{
	/**
	 * Input register.  This is unused for the LEDs.
	 */
	uint32_t read;
	/**
	 * The output register.  This is a bitmap of GPIO lines to set.
	 */
	uint32_t write;
	/**
	 * Write enable.  This sets the pins that should be controlled by `write`.
	 */
	uint32_t writeEnable;

	/**
	 * The index of the first GPIO pin connected to a LED.
	 */
	static constexpr uint32_t FirstLED = 14;
	/**
	 * The index of the last GPIO pin connected to a LED.
	 */
	static constexpr uint32_t LastLED = 15;
	/**
	 * The number of GPIO pins used for LEDs.
	 */
	static constexpr uint32_t LEDCount = LastLED - FirstLED + 1;

	/**
	 * Helper that defines a set of bits in a GPIO input word used for a
	 * specific purpose.
	 */
	struct InputBits
	{
		/**
		 * The number of bits.
		 */
		const uint32_t Count;

		/**
		 * The offset of the lowest bit in the GPIO input word.
		 */
		const uint32_t Shift;

		/**
		 * The mask used to extract the bits.
		 */
		[[nodiscard]] constexpr uint32_t mask() const
		{
			return (1 << Count) - 1;
		}

		/**
		 * Extract the bits for this range.
		 */
		[[nodiscard]] __always_inline uint32_t extract(uint32_t value) const
		{
			return (value >> Shift) & mask();
		}
	};

	/**
	 * The buttons zero to three are in the low four bits.
	 */
	static constexpr InputBits Buttons = {4, 4};

	/**
	 * Switches zero to three are in the next four bits.
	 */
	static constexpr InputBits Switches = {4, 0};

	/**
	 * Helper to read all of the bits defined by an `InputBits` instance.
	 */
	template<InputBits Bits>
	__always_inline uint32_t bits() volatile
	{
		return Bits.extract(read);
	}

	/**
	 * Helper to read a single bit defined by an `InputBits` instance.
	 */
	template<InputBits Bits>
	__always_inline uint32_t bit(uint32_t index) volatile
	{
		return (Bits.extract(read) >> index) & 1;
	}

	/**
	 * Read the value of all of the buttons.
	 */
	uint32_t buttons() volatile
	{
		return bits<Buttons>();
	}

	/**
	 * Read the value of all of the switches.
	 */
	uint32_t switches() volatile
	{
		return bits<Switches>();
	}

	/**
	 * Read the value of one of the buttons, indicated by index.
	 */
	uint32_t button(uint32_t index) volatile
	{
		return bit<Buttons>(index);
	}

	/**
	 * Read the value of one of the switches, indicated by index.
	 *
	 * This should be called `switch`, but that's a keyword in C/C++.
	 */
	uint32_t switch_value(uint32_t index) volatile
	{
		return bit<Switches>(index);
	}

	/**
	 * Helper that maps from an LED index to a bit to set / clear to control
	 * that LED.  Returns 0 for out-of-bounds LED values and so can be safely
	 * masked.
	 */
	constexpr static uint32_t led_bit(uint32_t index)
	{
		if (index >= LEDCount)
		{
			return 0;
		}
		return 1 << (index + FirstLED);
	}

	/**
	 * Enable  all of the LED GPIO pins.
	 *
	 * This must be called before `led_on` or `led_off`.
	 */
	void enable_all() volatile
	{
		uint32_t bitmap = 0;
		for (uint32_t i = 0; i < LEDCount; i++)
		{
			bitmap |= led_bit(i);
		}
		writeEnable = bitmap;
	}

	/**
	 * Turn on the LED specified by `index`.
	 */
	void led_on(uint32_t index) volatile
	{
		write = write | led_bit(index);
	}

	/**
	 * Turn off the LED specified by `index`.
	 */
	void led_off(uint32_t index) volatile
	{
		write = write & ~led_bit(index);
	}
};

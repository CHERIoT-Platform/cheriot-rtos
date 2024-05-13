#pragma once
#include <cdefs.h>
#include <stdint.h>

/**
 * Represents the state of the Sonata's joystick.
 *
 * Note, that up to three of the bits may be asserted at any given time.
 * There may be up to two cardinal directions asserted when the joystick is
 * pushed in a diagonal and the joystick may be pressed while being pushed in a
 * given direction.
 */
enum class SonataJoystick : uint8_t
{
	Left    = 1 << 0,
	Up      = 1 << 1,
	Pressed = 1 << 2,
	Down    = 1 << 3,
	Right   = 1 << 4,
};

/**
 * A Simple Driver for the Sonata's GPIO.
 *
 * Documentation source can be found at:
 * https://github.com/lowRISC/sonata-system/blob/1a59633d2515d4fe186a07d53e49ff95c18d9bbf/doc/ip/gpio.md
 *
 * Rendered documentation is served from:
 * https://lowrisc.org/sonata-system/doc/ip/gpio.html
 */
struct SonataGPIO
{
	uint32_t output;
	uint32_t input;
	uint32_t debouncedInput;
	uint32_t debouncedThreshold;
	uint32_t raspberryPiHeader;
	uint32_t raspberryPiMask;
	uint32_t arduinoShieldHeader;
	uint32_t arduinoShieldMask;

	/**
	 * The index of the first GPIO pin connected to a LED.
	 */
	static constexpr uint32_t FirstLED = 4;
	/**
	 * The index of the last GPIO pin connected to a LED.
	 */
	static constexpr uint32_t LastLED = 11;
	/**
	 * The number of GPIO pins used for LEDs.
	 */
	static constexpr uint32_t LEDCount = LastLED - FirstLED + 1;
	/**
	 * The mask covering the GPIO pins used for LEDs.
	 */
	static constexpr uint32_t LEDMask = ((1 << LEDCount) - 1) << FirstLED;

	constexpr static uint32_t led_bit(uint32_t index)
	{
		return (1 << (index + FirstLED)) & LEDMask;
	}

	void led_on(uint32_t index) volatile
	{
		output = output | led_bit(index);
	}

	void led_off(uint32_t index) volatile
	{
		output = output & ~led_bit(index);
	}

	void led_toggle(uint32_t index) volatile
	{
		output = output ^ led_bit(index);
	}

	/**
	 * Returns the state of the joystick.
	 */
	SonataJoystick read_joystick() volatile
	{
		return static_cast<SonataJoystick>(input & 0x1f);
	}
};

#pragma once
#include <cdefs.h>
#include <stdint.h>
#include <utils.hh>

/**
 * A simple driver for the Sonata's GPIO. This struct represents a single
 * GPIO instance, and the methods available to interact with that GPIO.
 *
 * Documentation source can be found at:
 * https://github.com/lowRISC/sonata-system/blob/9f794fe3bd4eec8d1a01ee81da97a7f2cec0b452/doc/ip/gpio.md
 *
 * Rendered documentation is served from:
 * https://lowrisc.org/sonata-system/doc/ip/gpio.html
 */
template<
  /**
   * The mask of bits of the `output` register that contain meaningful
   * GPIO output.
   */
  uint32_t OutputMask = 0xFFFF'FFFF,
  /**
   * The mask of bits of the `input` and `debouncedInput` registers that
   * contain meaningful GPIO input.
   */
  uint32_t InputMask = OutputMask,
  /**
   * Returns the bit corresponding to a given GPIO index. Will mask out
   * the bit if this is outside of the provided mask.
   */
  uint32_t OutputEnableMask = OutputMask>
struct SonataGpioBase : private utils::NoCopyNoMove
{
	uint32_t output;
	uint32_t input;
	uint32_t debouncedInput;
	uint32_t outputEnable;

	/**
	 * Returns a mask with a single bit set corresponding to the given GPIO index
	 * or 0 if that bit is not set in mask.
	 */
	constexpr static uint32_t gpio_bit(uint32_t index, uint32_t mask)
	{
		return (1 << index) & mask;
	}

	/**
	 * Set the output bit for a given GPIO pin index to a specified value.
	 * This will only have an effect if the corresponding bit is first set
	 * to `1` (i.e. output) in the `output_enable` register, and if the pin
	 * is a valid output pin.
	 */
	void set_output(uint32_t index, bool value) volatile
	{
		const uint32_t Bit = gpio_bit(index, OutputMask);
		if (value)
		{
			output = output | Bit;
		}
		else
		{
			output = output & ~Bit;
		}
	}

	/**
	 * Set the output enable bit for a given GPIO pin index. If `enable` is
	 * true, the GPIO pin is set to output. If `false`, it is instead set to
	 * input mode.
	 */
	void set_output_enable(uint32_t index, bool enable) volatile
	{
		const uint32_t Bit = gpio_bit(index, OutputEnableMask);
		if (enable)
		{
			outputEnable = outputEnable | Bit;
		}
		else
		{
			outputEnable = outputEnable & ~Bit;
		}
	}

	/**
	 * Read the input value for a given GPIO pin index. For this to be
	 * meaningful, the corresponding pin must be configured to be an input
	 * first (set output enable to `false` for the given index). If given an
	 * invalid GPIO pin (outside the input mask), then this value will
	 * always be false.
	 */
	bool read_input(uint32_t index) volatile
	{
		return (input & gpio_bit(index, InputMask)) > 0;
	}

	/**
	 * Read the debounced input value for a given GPIO pin index. For this
	 * to be meaningful, the corresponding pin must be configured to be an
	 * input first (set output enable to `false` for the given index). If
	 * given an invalid GPIO pin (outside the input mask), then this value
	 * will always be false.
	 */
	bool read_debounced_input(uint32_t index) volatile
	{
		return (debouncedInput & gpio_bit(index, InputMask)) > 0;
	}
};

/**
 * A driver for Sonata's Board GPIO (instance 0).
 *
 * Documentation source:
 * https://lowrisc.org/sonata-system/doc/ip/gpio.html
 */
struct SonataGpioBoard : SonataGpioBase<0x0000'00FF, 0x0001'FFFF, 0x0000'0000>
{
	/**
	 * Represents the state of Sonata's joystick, where each possible input
	 * corresponds to a given bit in the General GPIO's input register.
	 *
	 * Note that up to 3 of these bits may be asserted at any given time:
	 * pressing down the joystick whilst pushing it in a diagonal direction
	 * (i.e. 2 cardinal directions).
	 *
	 */
	enum class [[clang::flag_enum]] SonataJoystick : uint16_t
	{
		Left    = 1u << 8,
		Up      = 1u << 9,
		Pressed = 1u << 10,
		Down    = 1u << 11,
		Right   = 1u << 12,
	};

	/**
	 * The bit mappings of the output GPIO pins available in Sonata's General
	 * GPIO.
	 *
	 * Source: https://lowrisc.github.io/sonata-system/doc/ip/gpio.html
	 */
	enum Outputs : uint32_t
	{
		Leds = (0xFFu << 0),
	};

	/**
	 * The bit mappings of the input GPIO pins available in Sonata's General
	 * GPIO.
	 *
	 * Source: https://lowrisc.github.io/sonata-system/doc/ip/gpio.html
	 */
	enum Inputs : uint32_t
	{
		DipSwitches            = (0xFFu << 0),
		Joystick               = (0x1Fu << 8),
		SoftwareSelectSwitches = (0x7u << 13),
		MicroSdCardDetection   = (0x1u << 16),
	};

	/**
	 * The bit index of the first GPIO pin connected to a user LED.
	 */
	static constexpr uint32_t FirstLED = 0;
	/**
	 * The bit index of the last GPIO pin connected to a user LED.
	 */
	static constexpr uint32_t LastLED = 7;
	/**
	 * The number of user LEDs.
	 */
	static constexpr uint32_t LEDCount = LastLED - FirstLED + 1;
	/**
	 * The mask covering the GPIO pins used for user LEDs.
	 */
	static constexpr uint32_t LEDMask = static_cast<uint32_t>(Outputs::Leds);

	/**
	 * The output bit mask for a given user LED index
	 */
	constexpr static uint32_t led_bit(uint32_t index)
	{
		return gpio_bit(index + FirstLED, LEDMask);
	}

	/**
	 * Switches off the LED at the given user LED index
	 */
	void led_on(uint32_t index) volatile
	{
		output = output | led_bit(index);
	}

	/**
	 * Switches on the LED at the given user LED index
	 */
	void led_off(uint32_t index) volatile
	{
		output = output & ~led_bit(index);
	}

	/**
	 * Toggles the LED at the given user LED index
	 */
	void led_toggle(uint32_t index) volatile
	{
		output = output ^ led_bit(index);
	}

	/**
	 * The bit index of the first GPIO pin connected to a user switch.
	 */
	static constexpr uint32_t FirstSwitch = 0;
	/**
	 * The bit index of the last GPIO pin connected to a user switch.
	 */
	static constexpr uint32_t LastSwitch = 7;
	/**
	 * The number of user switches.
	 */
	static constexpr uint32_t SwitchCount = LastSwitch - FirstSwitch + 1;
	/**
	 * The mask covering the GPIO pins used for user switches.
	 */
	static constexpr uint32_t SwitchMask =
	  static_cast<uint32_t>(Inputs::DipSwitches);

	/**
	 * The input bit mask for a given user switch index
	 */
	constexpr static uint32_t switch_bit(uint32_t index)
	{
		return gpio_bit(index + FirstSwitch, SwitchMask);
	}

	/**
	 * Returns the value of the switch at the given user switch index.
	 */
	bool read_switch(uint32_t index) volatile
	{
		return (input & switch_bit(index)) > 0;
	}

	/**
	 * Returns the state of the joystick.
	 */
	SonataJoystick read_joystick() volatile
	{
		return static_cast<SonataJoystick>(input & Inputs::Joystick);
	}
};

using SonataGpioRaspberryPiHat = SonataGpioBase<0x0FFF'FFFF>;
using SonataGpioArduinoShield  = SonataGpioBase<0x0000'3FFF>;
using SonataGpioPmod           = SonataGpioBase<0x0000'00FF>;
using SonataGpioPmod0          = SonataGpioPmod;
using SonataGpioPmod1          = SonataGpioPmod;
using SonataGpioPmodC          = SonataGpioBase<0x0000'003F>;

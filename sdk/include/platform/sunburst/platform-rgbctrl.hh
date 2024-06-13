#pragma once
#include <cdefs.h>
#include <stdint.h>

/**
 * An enum representing each of the Sonata's RGB LEDs.
 */
enum class SonataRgbLed
{
	Led0,
	Led1,
};

/**
 * A driver for the Sonata's RGB LED Controller
 */
struct SonataRgbLedController
{
	/**
	 * Registers for setting the 8-bit red, green, and blue values
	 * for the two RGB Leds.
	 */
	uint32_t rgbLed0;
	uint32_t rgbLed1;
	/// Control Register
	uint32_t control;
	/// Status Register
	uint32_t status;

	/// Control Register Fields
	enum [[clang::flag_enum]] : uint32_t{
	  /// Write 1 to set RGB LEDs to specified colours.
	  ControlSet = 1 << 0,
	  /**
	   * Write 1 to turn off RGB LEDs.
	   * Write to ControlSet to turn on again.
	   */
	  ControlOff = 1 << 1,
	};

	/// Status Register Fields
	enum [[clang::flag_enum]] : uint32_t{
	  /**
	   * When asserted controller is idle and new colours can be set,
	   * otherwise writes to regLed0, regLed1, and control are ignored.
	   */
	  StatusIdle = 1 << 0,
	};

	void wait_for_idle() volatile
	{
		while ((status & StatusIdle) == 0) {}
	}

	void
	rgb(uint8_t red, uint8_t green, uint8_t blue, SonataRgbLed led) volatile
	{
		uint32_t rgb = (static_cast<uint32_t>(blue) << 16) |
		               (static_cast<uint32_t>(green) << 8) |
		               static_cast<uint32_t>(red);
		wait_for_idle();

		switch (led)
		{
			case SonataRgbLed::Led0:
				rgbLed0 = rgb;
			case SonataRgbLed::Led1:
				rgbLed1 = rgb;
		};
	}

	void update() volatile
	{
		wait_for_idle();
		control = ControlSet;
	}

	void clear() volatile
	{
		wait_for_idle();
		control = ControlOff;
	}
};

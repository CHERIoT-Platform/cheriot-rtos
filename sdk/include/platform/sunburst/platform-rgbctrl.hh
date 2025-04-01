#pragma once
#include <cdefs.h>
#include <stdint.h>
#include <utils.hh>

/**
 * An enum representing each of the Sonata's RGB LEDs.
 */
enum class SonataRgbLed
{
	Led0 = 0,
	Led1 = 1,
};

/**
 * A driver for the Sonata's RGB LED Controller
 */
struct SonataRgbLedController : private utils::NoCopyNoMove
{
	/**
	 * Registers for setting the 8-bit red, green, and blue values
	 * for the two RGB Leds.
	 */
	uint32_t ledColors[2];
	/**
	 * Control Register. See `SonataRgbLedController::ControlFields` for the
	 * fields.
	 */
	uint32_t control;
	/**
	 * Status Register See `SonataRgbLedController::StatusFields` for the
	 * fields.
	 */
	uint32_t status;

	/// Control Register Fields
	enum [[clang::flag_enum]] ControlFields : uint32_t
	{
		/// Write 1 to set RGB LEDs to specified colours.
		ControlSet = 1 << 0,
		/**
		 * Write 1 to turn off RGB LEDs.
		 * Write to ControlSet to turn on again.
		 */
		ControlOff = 1 << 1,
	};

	/// Status Register Fields
	enum [[clang::flag_enum]] StatusFields : uint32_t
	{
		/**
		 * When asserted controller is idle and new colours can be set,
		 * otherwise writes to regLed0, regLed1, and control are ignored.
		 */
		StatusIdle = 1 << 0,
	};

	/**
	 * Blocks until the controller is not busy.
	 *
	 * The controller can be busy when it is in the process of updating the
	 * LEDs. While busy, register writes will be ignored.
	 */
	void wait_for_idle() volatile
	{
		while ((status & StatusIdle) == 0) {}
	}

	/**
	 * Set the desired Red, Green, and Blue value of an LED. To apply these
	 * changes, one needs to run `SonataRgbLedController::update()`.
	 */
	void
	rgb(SonataRgbLed led, uint8_t red, uint8_t green, uint8_t blue) volatile
	{
		wait_for_idle();
		ledColors[static_cast<uint32_t>(led)] =
		  (static_cast<uint32_t>(blue) << 16) |
		  (static_cast<uint32_t>(green) << 8) | static_cast<uint32_t>(red);
	}

	/// Update the colours of the LEDs.
	void update() volatile
	{
		wait_for_idle();
		control = ControlSet;
	}

	/// Switch all of the RGB LEDs off.
	void off() volatile
	{
		wait_for_idle();
		control = ControlOff;
	}
};

static_assert(sizeof(SonataRgbLedController) == 16,
              "The SonataRgbLedController structure is the wrong size.");

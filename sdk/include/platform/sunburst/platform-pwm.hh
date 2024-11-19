#pragma once
#include <array>
#include <debug.hh>
#include <stdint.h>
#include <utils.hh>

namespace SonataPulseWidthModulation
{
	/**
	 * A driver for Sonata's Pulse-Width Modulation (PWM).
	 *
	 * Documentation source can be found at:
	 * https://github.com/lowRISC/sonata-system/blob/97a525c48f7bf051b999d0178dba04859819bc5e/doc/ip/pwm.md
	 *
	 * Rendered documentation is served from:
	 * https://lowrisc.github.io/sonata-system/doc/ip/pwm.html
	 */
	struct Output : private utils::NoCopyNoMove
	{
		/**
		 * The duty cycle of the wave, represented as a width counter. That
		 * is, the number of clock cycles for which the signal will be on. The
		 * duty cycle as a percentage is (duty cycle / period) * 100.
		 */
		uint32_t dutyCycle;

		/**
		 * The period (width) of the output block wave, set with the number of
		 * clock cycles that one period should last. The maximum period is 255
		 * as only an 8 bit counter is being used.
		 */
		uint32_t period;

		/*
		 * Sets the output of a specified pulse-width modulated output.
		 *
		 * @param period The length of the output wave represented as a counter
		 * of system clock cycles.
		 * @param dutyCycle The number of clock cycles for which a high pulse is
		 * sent within that period.
		 *
		 * So for example `output_set(0, 200, 31)` should set a 15.5% output.
		 * For a constant high output (100% duty cycle), set the dutyCycle >
		 * period.
		 */
		void output_set(uint8_t period, uint8_t dutyCycle) volatile
		{
			this->period    = period;
			this->dutyCycle = dutyCycle;
		}
	};

	/// A convenience structure that can map onto multiple PWM outputs.
	template<size_t NumberOfPwms = 6>
	struct Array
	{
		Output output[NumberOfPwms];

		template<size_t Index>
		volatile Output *get() volatile
		{
			static_assert(Index < NumberOfPwms, "PWM index out of bounds");
			return output + Index;
		}
	};

	/**
	 * There are six general purpose PWM outputs are general purpose that can be
	 * pinmuxed to different outputs.
	 */
	using General = Array<6>;
	/// There is one dedicated PWM for the LCD backlight.
	using Lcd = Output;
} // namespace SonataPulseWidthModulation

#pragma once
#include <debug.hh>
#include <stdint.h>

/**
 * A driver for Sonata's Pulse-Width Modulation (PWM).
 *
 * Documentation source can be found at:
 * https://github.com/lowRISC/sonata-system/blob/97a525c48f7bf051b999d0178dba04859819bc5e/doc/ip/pwm.md
 *
 * Rendered documentation is served from:
 * https://lowrisc.github.io/sonata-system/doc/ip/pwm.html
 */
struct SonataPulseWidthModulation
{
	/**
	 * Flag to set when debugging the driver for UART log messages.
	 */
	static constexpr bool DebugDriver = false;

	/**
	 * Helper for conditional debug logs and assertions.
	 */
	using Debug = ConditionalDebug<DebugDriver, "PWM">;

	/**
	 * The number of pulse-width modulated outputs that are available.
	 */
	static constexpr size_t OutputCount = 1;

	/**
	 * The pulse-width modulation outputs available on Sonata.
	 */
	struct OutputRegisters
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
	} outputs[OutputCount];

	/*
	 * Sets the output of a specified pulse-width modulated output.
	 *
	 * The first argument is the index of the output. The second argument is
	 * the period (length) of the output wave represented as a counter of
	 * system clock cycles. The third argument is the number of clock cycles
	 * for which a high pulse is sent within that period.
	 *
	 * So for example `output_set(0, 200, 31)` should set a 15.5% output.
	 */
	void output_set(uint32_t index, uint8_t period, uint8_t dutyCycle) volatile
	{
		Debug::Assert(index < OutputCount, "Specified PWM is out of range");
		Debug::Assert(dutyCycle <= period, "Duty cycle cannot exceed 100%");
		outputs[index].period    = period;
		outputs[index].dutyCycle = dutyCycle;
	}
};

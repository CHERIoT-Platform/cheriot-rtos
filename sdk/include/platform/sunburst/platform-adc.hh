#pragma once
#include <debug.hh>
#include <stdint.h>
#include <utils.hh>

/**
 * A simple driver for Sonata's XADC (Xilinx Analogue to Digital Converter).
 *
 * Documentation source can be found at:
 * https://github.com/lowRISC/sonata-system/blob/97a525c48f7bf051b999d0178dba04859819bc5e/doc/ip/adc.md
 *
 * Rendered documentation is served from:
 * https://lowrisc.github.io/sonata-system/doc/ip/adc.html
 */
class SonataAnalogueDigitalConverter : private utils::NoCopyNoMove
{
	/**
	 * Flag to set when debugging the driver for UART log messages.
	 */
	static constexpr bool DebugDriver = true;

	/**
	 * Helper for conditional debug logs and assertions.
	 */
	using Debug = ConditionalDebug<DebugDriver, "ADC">;

	/**
	 * Results of measurements / analogue conversions are stored in Dynamic
	 * Reconfiguration Port (DRP) status registers as most-significant-bit
	 * justified 12 bit values, represented by this mask.
	 */
	static constexpr uint16_t MeasurementMask = 0xFFF0;

	/**
	 * The location (offset) of the Xilinx Analogue-to-Digital Converter's
	 * Dynamic Reconfiguration Port (DRP) registers, which are 16-bit registers
	 * that are sequentially mapped to memory in 4-byte (word) intervals, in the
	 * lower 2 bytes of each word. This includes both status (read-only) and
	 * control (read/write) registers.
	 *
	 * https://docs.amd.com/r/en-US/ug480_7Series_XADC/XADC-Register-Interface
	 */
	enum class RegisterOffset : uint8_t
	{
		Temperature                      = 0x00,
		VoltageInternalSupply            = 0x01,
		VoltageAuxiliarySupply           = 0x02,
		VoltageDedicated                 = 0x03,
		VoltageInternalPositiveReference = 0x04,
		VoltageInternalNegativeReference = 0x05,
		VoltageBlockRamSupply            = 0x06,

		/* Offset 0x07 is undefined. */

		/* ADC A's calibration coefficient status registers omitted. */

		/* Offsets 0x0B and 0x0C are undefined. */

		/* Zynq-700 SoC-specific voltage status registers omitted. */

		VoltageAuxiliary0  = 0x10,
		VoltageAuxiliary1  = 0x11,
		VoltageAuxiliary2  = 0x12,
		VoltageAuxiliary3  = 0x13,
		VoltageAuxiliary4  = 0x14,
		VoltageAuxiliary5  = 0x15,
		VoltageAuxiliary6  = 0x16,
		VoltageAuxiliary7  = 0x17,
		VoltageAuxiliary8  = 0x18,
		VoltageAuxiliary9  = 0x19,
		VoltageAuxiliary10 = 0x1A,
		VoltageAuxiliary11 = 0x1B,
		VoltageAuxiliary12 = 0x1C,
		VoltageAuxiliary13 = 0x1D,
		VoltageAuxiliary14 = 0x1E,
		VoltageAuxiliary15 = 0x1F,

		/* Maximum & minimum sensor measurement status registers omitted. */

		/* Offsets 0x2B and 0x2F are undefined. */

		/* ADC B's calibration coefficient status registers omitted. */

		/* Offsets 0x33 to 0x3E are undefined. */

		FlagRegister    = 0x3F,
		ConfigRegister0 = 0x40,
		ConfigRegister1 = 0x41,
		ConfigRegister2 = 0x42,

		/* 0x43 to 0x47 are factory test registers and so are omitted. */

		/* Sequence and Alarm control registers are emitted. */
	};

	/**
	 * Definitions of fields (and their locations) within the Xilinx
	 * Analogue-to-digital Converter's Config Register 2 (offset 0x42).
	 *
	 * https://docs.amd.com/r/en-US/ug480_7Series_XADC/Control-Registers?section=XREF_53021_Configuration
	 */
	enum ConfigRegister2Field : uint16_t
	{
		/* Bits 0-3 are invalid and should not be interacted with. */

		/**
		 * Power-down bits for the Analogue-to-Digital Converter.
		 */
		PowerDownMask = 0x3 << 4,

		/* Bits 6-7 are invalid and should not be interacted with. */

		/**
		 * Bits used to select the division ratio between the Dynamic
		 * Reconfiguration Port clock (DCLK) and the lower frequency
		 * Analogue-to- Digital Converter Clock (ADCCLK). Values of 0 and 1 are
		 * mapped to a divider of 2 by the DCLK divider selection specification.
		 * All other values are mapped identically; the minimum division ratio
		 * is 2.
		 */
		ClockDividerMask = 0xFF << 8,
	};

	/**
	 * A helper that returns a pointer to the Analogue-to-Digital Converter
	 * (ADC)'s memory, which is mapped to the Dynamic Reconfiguration Port (DRP)
	 * registers used by the ADC.
	 */
	[[nodiscard, gnu::always_inline]] volatile uint32_t *registers() const
	{
		return MMIO_CAPABILITY(uint32_t, adc);
	}

	/**
	 * Read the contents of a Dynamic Reconfiguration Port (DRP) register from
	 * memory. DRP registers are 16 bits wide but are mapped sequentially in
	 * memory, using 4 bytes per register, with the relevant data written in the
	 * lower 16 bits.
	 *
	 * The single argument is the register to read the value of.
	 */
	[[nodiscard]] uint16_t register_read(RegisterOffset reg) const
	{
		uint8_t registerOffset = static_cast<uint8_t>(reg);
		return static_cast<uint16_t>(registers()[registerOffset]);
	}

	/**
	 * Write to the contents of a Dynamic Reconfiguration Port (DRP) register in
	 * memory. DRP Registers are 16 bits wide but are mapped sequentially in
	 * memory, using 4 bytes per register, with the relevant data written to the
	 * lower 16 bits.
	 *
	 * The first argument is the register to write to.
	 * The second argument is the value to write to the register.
	 */
	void register_write(RegisterOffset reg, uint16_t value) const
	{
		uint8_t registerOffset      = static_cast<uint8_t>(reg);
		registers()[registerOffset] = static_cast<uint32_t>(value);
	}

	/**
	 * Sets the relevant bits of a Dynamic Reconfiguration Port (DRP) register
	 * in memory. This acts like `register_write`, but additionally takes a
	 * mask so that it only overwrites specified bits of the register, and
	 * retains the value of all unselected bits.
	 *
	 * The first argument is the register to write to.
	 * The second argument is the mask of bits in the register to write to.
	 * The third argument is the values that will be written to the register
	 * according to the mask.
	 */
	void register_set_bits(RegisterOffset reg,
	                       uint16_t       bitMask,
	                       uint16_t       bitValues) const
	{
		uint16_t registerBits = register_read(reg);
		registerBits &= ~bitMask;            /* Clear bits in the mask. */
		registerBits |= bitMask & bitValues; /* Set values of masked bits. */
		return register_write(reg, registerBits);
	}

	public:
	/**
	 * Represents the offsets of the Xilinx Analogue-to-Digital Converter's
	 * (XADC) Dynamic Reconfiguration Port (DRP) status registers that are used
	 * to store values that are measured/sampled by the XADC, forming a mapping
	 * that provides a more comprehensible interface.
	 *
	 * https://lowrisc.github.io/sonata-system/doc/ip/adc.html
	 */
	enum class MeasurementRegister : uint8_t
	{
		ArduinoA0   = static_cast<uint8_t>(RegisterOffset::VoltageAuxiliary4),
		ArduinoA1   = static_cast<uint8_t>(RegisterOffset::VoltageAuxiliary12),
		ArduinoA2   = static_cast<uint8_t>(RegisterOffset::VoltageAuxiliary5),
		ArduinoA3   = static_cast<uint8_t>(RegisterOffset::VoltageAuxiliary13),
		ArduinoA4   = static_cast<uint8_t>(RegisterOffset::VoltageAuxiliary6),
		ArduinoA5   = static_cast<uint8_t>(RegisterOffset::VoltageAuxiliary14),
		Temperature = static_cast<uint8_t>(RegisterOffset::Temperature),
		VoltageInternalSupply =
		  static_cast<uint8_t>(RegisterOffset::VoltageInternalSupply),
		VoltageAuxiliarySupply =
		  static_cast<uint8_t>(RegisterOffset::VoltageAuxiliarySupply),
		VoltageInternalReferencePositive = static_cast<uint8_t>(
		  RegisterOffset::VoltageInternalPositiveReference),
		VoltageInternalReferenceNegative = static_cast<uint8_t>(
		  RegisterOffset::VoltageInternalNegativeReference),
		VoltageBlockRamSupply =
		  static_cast<uint8_t>(RegisterOffset::VoltageBlockRamSupply),
	};

	/**
	 * Possible power down modes that can be set in Config Register 2.
	 *
	 * https://docs.amd.com/r/en-US/ug480_7Series_XADC/Control-Registers?section=XREF_93518_Power_Down
	 */
	enum class PowerDownMode : uint8_t
	{
		None = 0b00, /* Default */
		/* 0b01 is not valid, and should not be selected. */
		ConverterB     = 0b10,
		BothConverters = 0b11,
	};

	/**
	 * The Xilinx Analogue-to-Digtal Converter can sample at a maximum rate of 1
	 * Megasample per second. It uses 16 bit Dynamic Reconfiguration Port (DRP)
	 * registers, and stores 12-bit measurements, which are stored most-
	 * significant-bit justified.
	 *
	 * https://docs.amd.com/r/en-US/ug480_7Series_XADC/XADC-Overview
	 */
	static constexpr size_t MaxSamples          = 1 * 1000 * 1000;
	static constexpr size_t RegisterSize        = 16;
	static constexpr size_t MeasurementBitWidth = 12;

	/**
	 * The minimum permissible Analogue-to-Digital Clock speed is 1 MHz, and the
	 * maximum is 26 MHz, as per the data sheet.
	 *
	 * See table 65 on page 57:
	 * https://docs.amd.com/v/u/en-US/ds181_Artix_7_Data_Sheet
	 */
	static constexpr size_t MinClockFrequencyHz = 1 * 1000 * 1000;
	static constexpr size_t MaxClockFrequencyHz = 26 * 1000 * 1000;

	/**
	 * A clock divider value to be used by the Xilinx Analogue-to-Digital
	 * Converter (XADC), which divides its input clock (the system clock)
	 * by this divider to determine the clock of the XADC. This clock
	 * must fall between specified maximum and minimum frequency, and the
	 * divider must be an 8-bit integer, greater than 2.
	 */
	typedef uint8_t ClockDivider;

	/**
	 * Constructor - initialises the MMIO capability for the Analogue-to-
	 * Digital converter, and then sets its clock divider and power down
	 * mode.
	 *
	 * Takes the clock divider and power down modes to initialise. The
	 * clock divider should divide the system clock to create a signal
	 * between 1 and 26 MHz, and should be at least 2.
	 */
	SonataAnalogueDigitalConverter(ClockDivider  divider,
	                               PowerDownMode powerDown)
	{
		set_clock_divider(divider);
		set_power_down(powerDown);
		/* The analogue-to-digital converter starts up in independent ADC mode
		by default, monitoring all channels, and so no further initialisation
		logic is needed. */
	}

	/**
	 * Constructor, without any manual setting of the clock divider. Initialises
	 * the MMIO capability for the Analogue-to-Digtal converter, and then sets
	 * its power down mode.
	 *
	 * Takes the power down mode to initialise.
	 */
	SonataAnalogueDigitalConverter(PowerDownMode powerDown)
	{
		set_power_down(powerDown);
		/* The Analogue-to-digital converter starts up in independent ADC mode
		by default, monitoring all channels, and so no further initialisation
		logic is needed. */
	}

	/**
	 * Sets the clock divider for the Sonata Analogue-to-Digital Converter
	 * (ADC), which is used to divide the system clock to create the ADC clock.
	 *
	 * The clock divider to use is provided as the single argument. It must be
	 * such that it creates a signal between 1 and 26 MHz, and should be at
	 * least 2.
	 */
	void set_clock_divider(ClockDivider divider)
	{
		Debug::Assert((divider >= 2), "The ADC divider must be at least 2");
		Debug::Assert(
		  (CPU_TIMER_HZ / divider >= MinClockFrequencyHz),
		  "The given divider causes the ADC clock to underclock its minimum.");
		Debug::Assert(
		  (CPU_TIMER_HZ / divider <= MaxClockFrequencyHz),
		  "The given divider causes the ADC clock to overclock its maximum.");
		register_set_bits(
		  RegisterOffset::ConfigRegister2, ClockDividerMask, (divider << 8));
	}

	/**
	 * Sets the power down configuration for the Sonata Analogue-to-Digital
	 * Converter (ADC). Calling this function allows either ADC B or the entire
	 * XADC to be **permanently** powered down.
	 *
	 * The power down mode to set is provided as an argument.
	 */
	void set_power_down(PowerDownMode powerdown)
	{
		register_set_bits(RegisterOffset::ConfigRegister2,
		                  PowerDownMask,
		                  (static_cast<uint16_t>(powerdown) << 4));
	}

	/**
	 * Reads the most recent output of the analogue-to-digital converter's
	 * measurements from a specified status register, corresponding to
	 * measurement of some channel. This currently assumes unipolar operation,
	 * which means all values are positive.
	 *
	 * The status register to read the last measurement of is given as the
	 * single argument.
	 *
	 * The output values can be translated to the relevant units being measured
	 * by using the transfer functions defined in documentation:
	 * https://docs.amd.com/r/en-US/ug480_7Series_XADC/ADC-Transfer-Functions
	 */
	[[nodiscard]] int16_t read_last_measurement(MeasurementRegister reg) const
	{
		uint16_t measurement = register_read(static_cast<RegisterOffset>(reg));
		measurement &= MeasurementMask;
		measurement >>= (RegisterSize - MeasurementBitWidth);

		/* Currently just simple logic that assumes a unipolar analogue
		measurement: to extend support to bipolar measurement, a mechanism for
		tracking whether a measurement is bipolar or not must be introduced, and
		if so, then the negative 12-bit two's complement values must be sign
		extended first. */
		return static_cast<int16_t>(measurement);
	}
};

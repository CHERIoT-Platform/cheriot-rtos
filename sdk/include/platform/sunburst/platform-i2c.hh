#pragma once
#include <cdefs.h>
#include <debug.hh>
#include <stdint.h>

/**
 * Driver for the OpenTitan's I2C block.
 *
 * Documentation source can be found at:
 * https://github.com/lowRISC/opentitan/tree/4fe1b8dd1a09af9dbc242434481ae031955dfd85/hw/ip/i2c
 */
struct OpenTitanI2c
{
	/// Interrupt State Register
	uint32_t interruptState;
	/// Interrupt Enable Register
	uint32_t interruptEnable;
	/// Interrupt Test Register
	uint32_t interruptTest;
	/// Alert Test Register (Unused in Sonata)
	uint32_t alertTest;
	/// I2C Control Register
	uint32_t control;
	/// I2C Live Status Register for Host and Target modes
	uint32_t status;
	/// I2C Read Data
	uint32_t readData;
	/// I2C Host Format Data
	uint32_t formatData;
	/// I2C FIFO control register
	uint32_t fifoCtrl;
	/// Host mode FIFO configuration
	uint32_t hostFifoConfiguration;
	/// Target mode FIFO configuration
	uint32_t targetFifoConfiguration;
	/// Host mode FIFO status register
	uint32_t hostFifoStatus;
	/// Target mode FIFO status register
	uint32_t targetFifoStatus;
	/// I2C Override Control Register
	uint32_t override;
	/// Oversampled Receive values
	uint32_t values;
	/**
	 * Detailed I2C Timings (directly corresponding to table 10 in the I2C
	 * Specification).
	 */
	uint32_t timing[5];
	/// I2C clock stretching timeout control.
	uint32_t timeoutControl;
	/// I2C target address and mask pairs
	uint32_t targetId;
	/// I2C target acquired data
	uint32_t acquiredData;
	/// I2C target transmit data
	uint32_t transmitData;
	/**
	 * I2C host clock generation timeout value (in units of input clock
	 * frequency).
	 */
	uint32_t hostTimeoutControl;
	/// I2C target internal stretching timeout control.
	uint32_t targetTimeoutControl;
	/**
	 * Number of times the I2C target has NACK'ed a new transaction since the
	 * last read of this register.
	 */
	uint32_t targetNackCount;
	/**
	 * Controls for mid-transfer (N)ACK phase handling.
	 */
	uint32_t targetAckControl;
	/// The data byte pending to be written to the Acquire (ACQ) FIFO.
	uint32_t acquireFifoNextData;
	/**
	 * Timeout in Host-Mode for an unhandled NACK before hardware automatically
	 * ends the transaction.
	 */
	uint32_t hostNackHandlerTimeout;
	/// Latched events that explain why the controller halted.
	uint32_t controllerEvents;
	/**
	 * Latched events that can cause the target module to stretch the clock at
	 * the beginning of a read transfer.
	 */
	uint32_t targetEvents;

	/**
	 * The interrupts of the OpenTitan's I2C block.
	 *
	 * Documentation source can be found at:
	 * https://github.com/lowRISC/opentitan/blob/4fe1b8dd1a09af9dbc242434481ae031955dfd85/hw/ip/i2c/doc/interfaces.md
	 */
	enum class Interrupt
	{
		/**
		 * A host mode interrupt. This is asserted whilst the Format FIFO level
		 * is below the low threshold. This is a level status interrupt.
		 */
		FormatThreshold,
		/**
		 * A host mode interrupt. This is asserted whilst the Receive FIFO level
		 * is above the high threshold. This is a level status interrupt.
		 */
		ReceiveThreshold,
		/**
		 * A target mode interrupt. This is asserted whilst the Aquired FIFO
		 * level is above the high threshold. This is a level status interrupt.
		 */
		AcquiredThreshold,
		/**
		 * A host mode interrupt. This is raised if the Receive FIFO has
		 * overflowed.
		 */
		ReceiveOverflow,
		/**
		 * A host mode interrupt. This is raised if the controller FSM is
		 * halted, such as on an unexpected NACK or lost arbitration. Check the
		 * `controllerEvents` register for the reason. The interrupt will only
		 * be released when the bits in `controllerEvents` are cleared.
		 */
		ControllerHalt,
		/**
		 * A host mode interrupt. This is raised if the SCL line drops early
		 * (not supported without clock synchronization).
		 */
		SclInterference,
		/**
		 * A host mode interrupt. This is raised if the SDA line goes low when
		 * host is trying to assert high.
		 */
		SdaInterference,
		/**
		 * A host mode interrupt. This is raised if target stretches the clock
		 * beyond the allowed timeout period.
		 */
		StretchTimeout,
		/**
		 * A host mode interrupt. This is raised if the target does not assert a
		 * constant value of SDA during transmission.
		 */
		SdaUnstable,
		/**
		 * A host and target mode interrupt. In host mode, raised if the host
		 * issues a repeated START or terminates the transaction by issuing
		 * STOP. In target mode, raised if the external host issues a STOP or
		 * repeated START.
		 */
		CommandComplete,
		/**
		 * A target mode interrupt. This is raised if the target is stretching
		 * clocks for a read command. This is a level status interrupt.
		 */
		TransmitStretch,
		/**
		 * A target mode interrupt. This is asserted whilst the Transmit FIFO
		 * level is below the low threshold. This is a level status interrupt.
		 */
		TransmitThreshold,
		/**
		 * A target mode interrupt. This is raised if the target is stretching
		 * clocks due to full Aquired FIFO or zero count in
		 * targetAckControl.NBYTES (if enabled). This is a level status
		 * interrupt.
		 */
		AcquiredFull,
		/**
		 * A target mode interrupt. This is raised if STOP is received without a
		 * preceding NACK during an external host read.
		 */
		UnexpectedStop,
		/**
		 * A target mode interrupt. This is raised if the host stops sending the
		 * clock during an ongoing transaction.
		 */
		HostTimeout,
	};

	static constexpr uint32_t interrupt_bit(const Interrupt Interrupt)
	{
		return 1 << static_cast<uint32_t>(Interrupt);
	};

	/// Control Register Fields
	enum [[clang::flag_enum]] : uint32_t{
	  /// Enable Host I2C functionality
	  ControlEnableHost = 1 << 0,
	  /// Enable Target I2C functionality
	  ControlEnableTarget = 1 << 1,
	  /// Enable I2C line loopback test If line loopback is enabled, the
	  /// internal design sees ACQ and RX data as "1"
	  ControlLineLoopback = 1 << 2,
	  /// Enable NACKing the address on a stretch timeout. This is a target
	  /// mode feature. If enabled, a stretch timeout will cause the device to
	  /// NACK the address byte. If disabled, it will ACK instead.
	  ControlNackAddressAfterTimeout = 1 << 3,
	  /// Enable ACK Control Mode, which works with the `targetAckControl`
	  /// register to allow software to control upper-layer (N)ACKing.
	  ControlAckControlEnable = 1 << 4,
	  /// Enable the bus monitor in multi-controller mode.
	  ControlMultiControllerMonitorEnable = 1 << 5,
	  /// If set, causes a read transfer addressed to the this target to set
	  /// the corresponding bit in the `targetEvents` register. While the
	  /// `transmitPending` field is 1, subsequent read transactions will
	  /// stretch the clock, even if there is data in the Transmit FIFO.
	  ControlTransmitStretchEnable = 1 << 6,
	};

	/// Status Register Fields
	enum [[clang::flag_enum]] : uint32_t{
	  /// Host mode Format FIFO is full
	  StatusFormatFull = 1 << 0,
	  /// Host mode Receive FIFO is full
	  StatusReceiveFull = 1 << 1,
	  /// Host mode Format FIFO is empty
	  StatusFormatEmpty = 1 << 2,
	  /// Host functionality is idle. No Host transaction is in progress
	  StatusHostIdle = 1 << 3,
	  /// Target functionality is idle. No Target transaction is in progress
	  StatusTargetIdle = 1 << 4,
	  /// Host mode Receive FIFO is empty
	  SmatusReceiveEmpty = 1 << 5,
	  /// Target mode Transmit FIFO is full
	  StatusTransmitFull = 1 << 6,
	  /// Target mode Acquired FIFO is full
	  StatusAcquiredFull = 1 << 7,
	  /// Target mode Transmit FIFO is empty
	  StatusTransmitEmpty = 1 << 8,
	  /// Target mode Acquired FIFO is empty
	  StatusAcquiredEmpty = 1 << 9,
	  /// Target mode stretching at (N)ACK phase due to zero count
	  /// in the `targetAckControl` register.
	  StatusAckControlStretch = 1 << 10,
	};

	/// FormatData Register Fields
	enum [[clang::flag_enum]] : uint32_t{
	  /// Issue a START condition before transmitting BYTE.
	  FormatDataStart = 1 << 8,
	  /// Issue a STOP condition after this operation
	  FormatDataStop = 1 << 9,
	  /// Read BYTE bytes from I2C. (256 if BYTE==0)
	  FormatDataReadBytes = 1 << 10,
	  /**
	   * Do not NACK the last byte read, let the read
	   * operation continue
	   */
	  FormatDataReadCount = 1 << 11,
	  /// Do not signal an exception if the current byte is not ACK’d
	  FormatDataNakOk = 1 << 12,
	};

	/// FifoControl Register Fields
	enum [[clang::flag_enum]] : uint32_t{
	  /// Receive fifo reset. Write 1 to the register resets it. Read returns 0
	  FifoControlReceiveReset = 1 << 0,
	  /// Format fifo reset. Write 1 to the register resets it. Read returns 0
	  FifoControlFormatReset = 1 << 1,
	  /// Acquired FIFO reset. Write 1 to the register resets it. Read returns 0
	  FifoControlAcquiredReset = 1 << 7,
	  /// Transmit FIFO reset. Write 1 to the register resets it. Read returns 0
	  FifoControlTransmitReset = 1 << 8,
	};

	/// ControllerEvents Register Fields
	enum [[clang::flag_enum]] : uint32_t{
	  /// Controller FSM is halted due to receiving an unexpected NACK.
	  ControllerEventsNack = 1 << 0,
	  /**
	   * Controller FSM is halted due to a Host-Mode active transaction being
	   * ended by the `hostNackHandlerTimeout` mechanism.
	   */
	  ControllerEventsUnhandledNackTimeout = 1 << 1,
	  /**
	   * Controller FSM is halted due to a Host-Mode active transaction being
	   * terminated because of a bus timeout activated by `timeoutControl`.
	   */
	  ControllerEventsBusTimeout = 1 << 2,
	  /**
	   * Controller FSM is halted due to a Host-Mode active transaction being
	   * terminated because of lost arbitration.
	   */
	  ControllerEventsArbitrationLost = 1 << 3,
	};

	// Referred to as 'RX FIFO' in the documentation
	static constexpr uint32_t ReceiveFifoDepth = 8;

	/// Flag set when we're debugging this driver.
	static constexpr bool DebugOpenTitanI2c = true;

	/// Helper for conditional debug logs and assertions.
	using Debug = ConditionalDebug<DebugOpenTitanI2c, "OpenTitan I2C">;

	/**
	 * Performs a 32-bit integer unsigned division, rounding up. The bottom
	 * 16 bits of the result are then returned.
	 *
	 * As usual, a divisor of 0 is still Undefined Behavior.
	 */
	static uint16_t round_up_divide(uint32_t a, uint32_t b)
	{
		if (a == 0)
		{
			return 0;
		}
		const uint32_t Res = ((a - 1) / b) + 1;
		Debug::Assert(Res <= UINT16_MAX,
		              "Division result too large to fit in uint16_t.");
		return static_cast<uint16_t>(Res);
	}

	/**
	 * Reset the controller Events so that the `ControllerHalt` interrupt will
	 * be released, allowing the I2C driver to continue after the controller
	 * FSM has halted due to e.g. a NACK, configured timeout, or loss of
	 * arbitration.
	 *
	 * Returns the (masked) result of reading the register before clearing
	 * the controller events, so that you can use the `controllerEvent`
	 * register fields to determine why the Controller FSM was halted.
	 */
	uint32_t reset_controller_events() volatile
	{
		constexpr uint32_t FieldMask =
		  (ControllerEventsNack | ControllerEventsUnhandledNackTimeout |
		   ControllerEventsBusTimeout | ControllerEventsArbitrationLost);
		uint32_t events  = controllerEvents & FieldMask;
		controllerEvents = FieldMask;
		return events;
	}

	/// Reset all of the fifos.
	void reset_fifos() volatile
	{
		fifoCtrl = (FifoControlReceiveReset | FifoControlFormatReset |
		            FifoControlAcquiredReset | FifoControlTransmitReset);
	}

	/// Configure the I2C block to be in host mode.
	void host_mode_set() volatile
	{
		control = ControlEnableHost;
	}

	/**
	 * Set the I2C timing parameters appropriately for the given bit rate.
	 * Distilled from:
	 * https://github.com/lowRISC/opentitan/blob/9ddf276c64e2974ed8e528e8b2feb00b977861de/hw/ip/i2c/doc/programmers_guide.md
	 */
	void speed_set(const uint32_t SpeedKhz) volatile
	{
		// We must round up the system clock frequency to lengthen intervals.
		const uint16_t SystemClockKhz = round_up_divide(CPU_TIMER_HZ, 1000);
		// We want to underestimate the clock period, to lengthen the timings.
		const uint16_t ClockPeriod = (1000 * 1000) / SystemClockKhz;

		// Decide which bus mode this represents
		uint32_t mode = (SpeedKhz > 100u) + (SpeedKhz > 400u);

		// Minimum fall time when V_DD is 3.3V
		constexpr uint16_t MinimumFallTime = 20 * 3 / 5;
		// Specification minimum timings (Table 10) in nanoseconds for each bus
		// mode.
		constexpr uint16_t MinimumTimeValues[5][2][3] = {
		  {
		    {4700u, 1300u, 150u}, // Low Period
		    {4000u, 600u, 260u},  // High Period
		  },
		  {
		    // Fall time of SDA and SCL signals
		    {MinimumFallTime, MinimumFallTime, MinimumFallTime},
		    // Rise time of SDA and SCL signals
		    {120, 120, 120},
		  },
		  {
		    {4700u, 600u, 260u}, // Hold time for a repeated start condition
		    {4000u, 600u, 260u}, // Set-up time for a repeated start condition
		  },
		  {
		    {4000u, 1u, 1u},   // Data hold time
		    {500u, 100u, 50u}, // Data set-up time
		  },
		  {
		    // Bus free time between a STOP and START condition
		    {4700u, 1300u, 500u},
		    // Set-up time for a STOP condition
		    {4000u, 600u, 260u},
		  },
		};
		for (uint32_t i = 0; i < 5; ++i)
		{
			timing[i] =
			  (round_up_divide(MinimumTimeValues[i][0][mode], ClockPeriod)
			   << 16) |
			  round_up_divide(MinimumTimeValues[i][1][mode], ClockPeriod);
		}
	}

	void blocking_write_byte(const uint32_t Fmt) volatile
	{
		while (0 != (StatusFormatFull & status)) {}
		formatData = Fmt;
	}

	/// Returns true when the format fifo is empty
	[[nodiscard]] bool format_is_empty() volatile
	{
		return 0 != (StatusFormatEmpty & status);
	}

	[[nodiscard]] bool blocking_write(const uint8_t  Addr7,
	                                  const uint8_t  data[],
	                                  const uint32_t NumBytes,
	                                  const bool     SkipStop) volatile
	{
		if (NumBytes == 0)
		{
			return true;
		}
		blocking_write_byte(FormatDataStart | (Addr7 << 1) | 0u);
		for (uint32_t i = 0; i < NumBytes - 1; ++i)
		{
			blocking_write_byte(data[i]);
		}
		blocking_write_byte((SkipStop ? 0u : FormatDataStop) |
		                    data[NumBytes - 1]);
		while (!format_is_empty())
		{
			if (interrupt_is_asserted(Interrupt::ControllerHalt))
			{
				reset_controller_events();
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] bool blocking_read(const uint8_t  Addr7,
	                                 uint8_t        buf[],
	                                 const uint32_t NumBytes) volatile
	{
		for (uint32_t idx = 0; idx < NumBytes; idx += ReceiveFifoDepth)
		{
			blocking_write_byte(FormatDataStart | (Addr7 << 1) | 1u);
			while (!format_is_empty()) {}
			if (interrupt_is_asserted(Interrupt::ControllerHalt))
			{
				reset_controller_events();
				return false;
			}
			uint32_t bytesRemaining = NumBytes - idx;
			bool     lastChunk      = ReceiveFifoDepth >= bytesRemaining;
			uint8_t chunkSize = lastChunk ? static_cast<uint8_t>(bytesRemaining)
			                              : ReceiveFifoDepth;

			blocking_write_byte((lastChunk ? FormatDataStop : 0) |
			                    FormatDataReadBytes | chunkSize);
			while (!format_is_empty()) {}

			for (uint32_t chunkIdx = 0; chunkIdx < chunkSize; ++chunkIdx)
			{
				buf[idx + chunkIdx] = readData;
			}
		}
		return true;
	}

	/// Returns true if the given interrupt is asserted.
	[[nodiscard]] bool interrupt_is_asserted(Interrupt interrupt) volatile
	{
		return 0 != (interruptState & interrupt_bit(interrupt));
	}

	/// Clears the given interrupt.
	void interrupt_clear(Interrupt interrupt) volatile
	{
		interruptState = interrupt_bit(interrupt);
	}

	/// Enables the given interrupt.
	void interrupt_enable(Interrupt interrupt) volatile
	{
		interruptEnable = interruptEnable | interrupt_bit(interrupt);
	}

	/// Disables the given interrupt.
	void interrupt_disable(Interrupt interrupt) volatile
	{
		interruptEnable = interruptEnable & ~interrupt_bit(interrupt);
	}

	/**
	 * Sets the thresholds for the format and receive fifos.
	 */
	void host_thresholds_set(uint16_t formatThreshold,
	                         uint16_t receiveThreshold) volatile
	{
		hostFifoConfiguration =
		  (formatThreshold & 0xfff) << 16 | (receiveThreshold & 0xfff);
	}
};

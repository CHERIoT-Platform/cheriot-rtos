#pragma once
#include <array>
#include <cheri.hh>
#include <cstddef>
#include <cstdint>
#include <debug.hh>
#include <futex.h>
#include <interrupt.h>
#include <locks.hh>
#include <optional>
#include <platform/concepts/ethernet.hh>
#include <platform/sunburst/platform-gpio.hh>
#include <platform/sunburst/platform-spi.hh>
#include <thread.h>
#include <type_traits>

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(EthernetInterruptCapability,
                                        InterruptName::EthernetInterrupt,
                                        true,
                                        true);

/**
 * The driver for KSZ8851 SPI Ethernet MAC.
 */
class Ksz8851Ethernet
{
	/**
	 * Flag set when we're debugging this driver.
	 */
	static constexpr bool DebugEthernet = false;

	/**
	 * Flag set to log messages when frames are dropped.
	 */
	static constexpr bool DebugDroppedFrames = true;

	/**
	 * Maxmium size of a single Ethernet frame.
	 */
	static constexpr uint16_t MaxFrameSize = 1500;

	/**
	 * Helper for conditional debug logs and assertions.
	 */
	using Debug = ConditionalDebug<DebugEthernet, "Ethernet driver">;

	/**
	 * Helper for conditional debug logs and assertions for dropped frames.
	 */
	using DebugFrameDrops =
	  ConditionalDebug<DebugDroppedFrames, "Ethernet driver">;

	/**
	 * Import the Capability helper from the CHERI namespace.
	 */
	template<typename T>
	using Capability = CHERI::Capability<T>;

	/**
	 * GPIO output pins to be used
	 */
	enum class GpioPin : uint8_t
	{
		EthernetChipSelect = 13,
		EthernetReset      = 14,
	};

	/**
	 * SPI commands
	 */
	enum class SpiCommand : uint8_t
	{
		ReadRegister  = 0b00,
		WriteRegister = 0b01,
		// DMA in this context means that the Ethernet MAC is DMA directly
		// from the SPI interface into its internal buffer, so it takes single
		// SPI transaction for the entire frame. It is unrelated to whether
		// SPI driver uses PIO or DMA for the SPI transaction.
		ReadDma  = 0b10,
		WriteDma = 0b11,
	};

	/**
	 * The location of registers
	 */
	enum class RegisterOffset : uint8_t
	{
		ChipConfiguration = 0x08,
		MacAddressLow     = 0x10,
		MacAdressMiddle   = 0x12,
		MacAddressHigh    = 0x14,
		OnChipBusControl  = 0x20,
		EepromControl     = 0x22,
		MemoryBistInfo    = 0x24,
		GlobalReset       = 0x26,

		/* Wakeup frame registers omitted */

		TransmitControl               = 0x70,
		TransmitStatus                = 0x72,
		ReceiveControl1               = 0x74,
		ReceiveControl2               = 0x76,
		TransmitQueueMemoryInfo       = 0x78,
		ReceiveFrameHeaderStatus      = 0x7C,
		ReceiveFrameHeaderByteCount   = 0x7E,
		TransmitQueueCommand          = 0x80,
		ReceiveQueueCommand           = 0x82,
		TransmitFrameDataPointer      = 0x84,
		ReceiveFrameDataPointer       = 0x86,
		ReceiveDurationTimerThreshold = 0x8C,
		ReceiveDataByteCountThreshold = 0x8E,
		InterruptEnable               = 0x90,
		InterruptStatus               = 0x92,
		ReceiveFrameCountThreshold    = 0x9c,
		TransmitNextTotalFrameSize    = 0x9E,

		/* MAC address hash table registers omitted */

		FlowControlLowWatermark               = 0xB0,
		FlowControlHighWatermark              = 0xB2,
		FlowControlOverrunWatermark           = 0xB4,
		ChipIdEnable                          = 0xC0,
		ChipGlobalControl                     = 0xC6,
		IndirectAccessControl                 = 0xC8,
		IndirectAccessDataLow                 = 0xD0,
		IndirectAccessDataHigh                = 0xD2,
		PowerManagementEventControl           = 0xD4,
		GoSleepWakeUp                         = 0xD4,
		PhyReset                              = 0xD4,
		Phy1MiiBasicControl                   = 0xE4,
		Phy1MiiBasicStatus                    = 0xE6,
		Phy1IdLow                             = 0xE8,
		Phy1High                              = 0xEA,
		Phy1AutoNegotiationAdvertisement      = 0xEC,
		Phy1AutoNegotiationLinkPartnerAbility = 0xEE,
		Phy1SpecialControlStatus              = 0xF4,
		Port1Control                          = 0xF6,
		Port1Status                           = 0xF8,
	};

	using MACAddress = std::array<uint8_t, 6>;

	/**
	 * Flag bits of the TransmitControl register.
	 */
	enum [[clang::flag_enum]] TransmitControl : uint16_t{
	  TransmitEnable                 = 1 << 0,
	  TransmitCrcEnable              = 1 << 1,
	  TransmitPaddingEnable          = 1 << 2,
	  TransmitFlowControlEnable      = 1 << 3,
	  FlushTransmitQueue             = 1 << 4,
	  TransmitChecksumGenerationIp   = 1 << 5,
	  TransmitChecksumGenerationTcp  = 1 << 6,
	  TransmitChecksumGenerationIcmp = 1 << 9,
	};

	/**
	 * Flag bits of the ReceiveControl1 register.
	 */
	enum [[clang::flag_enum]] ReceiveControl1 : uint16_t{
	  ReceiveEnable                                        = 1 << 0,
	  ReceiveInverseFilter                                 = 1 << 1,
	  ReceiveAllEnable                                     = 1 << 4,
	  ReceiveUnicastEnable                                 = 1 << 5,
	  ReceiveMulticastEnable                               = 1 << 6,
	  ReceiveBroadcastEnable                               = 1 << 7,
	  ReceiveMulticastAddressFilteringWithMacAddressEnable = 1 << 8,
	  ReceiveErrorFrameEnable                              = 1 << 9,
	  ReceiveFlowControlEnable                             = 1 << 10,
	  ReceivePhysicalAddressFilteringWithMacAddressEnable  = 1 << 11,
	  ReceiveIpFrameChecksumCheckEnable                    = 1 << 12,
	  ReceiveTcpFrameChecksumCheckEnable                   = 1 << 13,
	  ReceiveUdpFrameChecksumCheckEnable                   = 1 << 14,
	  FlushReceiveQueue                                    = 1 << 15,
	};

	/**
	 * Flag bits of the ReceiveControl2 register.
	 */
	enum [[clang::flag_enum]] ReceiveControl2 : uint16_t{
	  ReceiveSourceAddressFiltering            = 1 << 0,
	  ReceiveIcmpFrameChecksumEnable           = 1 << 1,
	  UdpLiteFrameEnable                       = 1 << 2,
	  ReceiveIpv4Ipv6UdpFrameChecksumEqualZero = 1 << 3,
	  ReceiveIpv4Ipv6FragmentFramePass         = 1 << 4,
	  DataBurst4Bytes                          = 0b000 << 5,
	  DataBurst8Bytes                          = 0b001 << 5,
	  DataBurst16Bytes                         = 0b010 << 5,
	  DataBurst32Bytes                         = 0b011 << 5,
	  DataBurstSingleFrame                     = 0b100 << 5,
	};

	/**
	 * Flag bits of the ReceiveFrameHeaderStatus register.
	 */
	enum [[clang::flag_enum]] ReceiveFrameHeaderStatus : uint16_t{
	  ReceiveCrcError                = 1 << 0,
	  ReceiveRuntFrame               = 1 << 1,
	  ReceiveFrameTooLong            = 1 << 2,
	  ReceiveFrameType               = 1 << 3,
	  ReceiveMiiError                = 1 << 4,
	  ReceiveUnicastFrame            = 1 << 5,
	  ReceiveMulticastFrame          = 1 << 6,
	  ReceiveBroadcastFrame          = 1 << 7,
	  ReceiveUdpFrameChecksumStatus  = 1 << 10,
	  ReceiveTcpFrameChecksumStatus  = 1 << 11,
	  ReceiveIpFrameChecksumStatus   = 1 << 12,
	  ReceiveIcmpFrameChecksumStatus = 1 << 13,
	  ReceiveFrameValid              = 1 << 15,
	};

	/**
	 * Flag bits of the ReceiveQueueCommand register.
	 */
	enum [[clang::flag_enum]] ReceiveQueueCommand : uint16_t{
	  ReleaseReceiveErrorFrame            = 1 << 0,
	  StartDmaAccess                      = 1 << 3,
	  AutoDequeueReceiveQueueFrameEnable  = 1 << 4,
	  ReceiveFrameCountThresholdEnable    = 1 << 5,
	  ReceiveDataByteCountThresholdEnable = 1 << 6,
	  ReceiveDurationTimerThresholdEnable = 1 << 7,
	  ReceiveIpHeaderTwoByteOffsetEnable  = 1 << 9,
	  ReceiveFrameCountThresholdStatus    = 1 << 10,
	  ReceiveDataByteCountThresholdstatus = 1 << 11,
	  ReceiveDurationTimerThresholdStatus = 1 << 12,
	};

	/**
	 * Flag bits of the TransmitQueueCommand register.
	 */
	enum [[clang::flag_enum]] TransmitQueueCommand : uint16_t{
	  ManualEnqueueTransmitQueueFrameEnable = 1 << 0,
	  TransmitQueueMemoryAvailableMonitor   = 1 << 1,
	  AutoEnqueueTransmitQueueFrameEnable   = 1 << 2,
	};

	/**
	 * Flag bits of the TransmitFrameDataPointer and ReceiveFrameDataPointer
	 * register.
	 */
	enum [[clang::flag_enum]] FrameDataPointer : uint16_t{
	  /**
	   * When this bit is set, the frame data pointer register increments
	   * automatically on accesses to the data register.
	   */
	  FrameDataPointerAutoIncrement = 1 << 14,
	};

	/**
	 * Flags bits of the InterruptStatus and InterruptEnable registers.
	 */
	enum [[clang::flag_enum]] Interrupt : uint16_t{
	  EnergyDetectInterrupt             = 1 << 2,
	  LinkupDetectInterrupt             = 1 << 3,
	  ReceiveMagicPacketDetectInterrupt = 1 << 4,
	  ReceiveWakeupFrameDetectInterrupt = 1 << 5,
	  TransmitSpaceAvailableInterrupt   = 1 << 6,
	  ReceiveProcessStoppedInterrupt    = 1 << 7,
	  TransmitProcessStoppedInterrupt   = 1 << 8,
	  ReceiveOverrunInterrupt           = 1 << 11,
	  ReceiveInterrupt                  = 1 << 13,
	  TransmitInterrupt                 = 1 << 14,
	  LinkChangeInterruptStatus         = 1 << 15,
	};

	/**
	 * Flags bits of the Port1Control register.
	 */
	enum [[clang::flag_enum]] Port1Control : uint16_t{
	  Advertised10BTHalfDuplexCapability  = 1 << 0,
	  Advertised10BTFullDuplexCapability  = 1 << 1,
	  Advertised100BTHalfDuplexCapability = 1 << 2,
	  Advertised100BTFullDuplexCapability = 1 << 3,
	  AdvertisedFlowControlCapability     = 1 << 4,
	  ForceDuplex                         = 1 << 5,
	  ForceSpeed                          = 1 << 6,
	  AutoNegotiationEnable               = 1 << 7,
	  ForceMDIX                           = 1 << 9,
	  DisableAutoMDIMDIX                  = 1 << 10,
	  RestartAutoNegotiation              = 1 << 13,
	  TransmitterDisable                  = 1 << 14,
	  LedOff                              = 1 << 15,
	};

	/**
	 * Flags bits of the Port1Status register.
	 */
	enum [[clang::flag_enum]] Port1Status : uint16_t{
	  Partner10BTHalfDuplexCapability  = 1 << 0,
	  Partner10BTFullDuplexCapability  = 1 << 1,
	  Partner100BTHalfDuplexCapability = 1 << 2,
	  Partner100BTFullDuplexCapability = 1 << 3,
	  PartnerFlowControlCapability     = 1 << 4,
	  LinkGood                         = 1 << 5,
	  AutoNegotiationDone              = 1 << 6,
	  MDIXStatus                       = 1 << 7,
	  OperationDuplex                  = 1 << 9,
	  OperationSpeed                   = 1 << 10,
	  PolarityReverse                  = 1 << 13,
	  HPMDIX                           = 1 << 15,
	};

	/**
	 * The futex used to wait for interrupts when packets are available to
	 * receive.
	 */
	const uint32_t *receiveInterruptFutex;

	/**
	 * Set value of a GPIO output.
	 */
	inline void set_gpio_output_bit(GpioPin pin, bool value) const
	{
		uint32_t shift  = static_cast<uint8_t>(pin);
		uint32_t output = gpio()->output;
		output &= ~(1 << shift);
		output |= value << shift;
		gpio()->output = output;
	}

	/**
	 * Read a register from the KSZ8851.
	 */
	[[nodiscard]] uint16_t register_read(RegisterOffset reg) const
	{
		// KSZ8851 command have the following format:
		//
		// First byte:
		// +---------+-------------+-------------------+
		// | 7     6 | 5         2 | 1               0 |
		// +---------+-------------+-------------------+
		// | Command | Byte Enable | Address (bit 7-6) |
		// +---------+-------------+-------------------+
		//
		// Second byte (for register read/write only):
		// +-------------------+--------+
		// | 7               4 | 3    0 |
		// +-------------------+--------+
		// | Address (bit 5-2) | Unused |
		// +-------------------+--------+
		//
		// Note that the access is 32-bit since bit 1 & 0 of the address is not
		// included. KSZ8851 have 16-bit registers so byte enable is used to
		// determine which register to access from the 32 bits specified by the
		// address.
		uint8_t addr       = static_cast<uint8_t>(reg);
		uint8_t byteEnable = (addr & 0x2) == 0 ? 0b0011 : 0b1100;
		uint8_t bytes[2];
		bytes[0] = (static_cast<uint8_t>(SpiCommand::ReadRegister) << 6) |
		           (byteEnable << 2) | (addr >> 6);
		bytes[1] = (addr << 2) & 0b11110000;

		set_gpio_output_bit(GpioPin::EthernetChipSelect, false);
		spi()->blocking_write(bytes, sizeof(bytes));
		uint16_t val;
		spi()->blocking_read(reinterpret_cast<uint8_t *>(&val), sizeof(val));
		set_gpio_output_bit(GpioPin::EthernetChipSelect, true);
		return val;
	}

	/**
	 * Write a register to KSZ8851.
	 */
	void register_write(RegisterOffset reg, uint16_t val) const
	{
		// See register_read for command format.
		uint8_t addr       = static_cast<uint8_t>(reg);
		uint8_t byteEnable = (addr & 0x2) == 0 ? 0b0011 : 0b1100;
		uint8_t bytes[2];
		bytes[0] = (static_cast<uint8_t>(SpiCommand::WriteRegister) << 6) |
		           (byteEnable << 2) | (addr >> 6);
		bytes[1] = (addr << 2) & 0b11110000;

		set_gpio_output_bit(GpioPin::EthernetChipSelect, false);
		spi()->blocking_write(bytes, sizeof(bytes));
		spi()->blocking_write(reinterpret_cast<uint8_t *>(&val), sizeof(val));
		spi()->wait_idle();
		set_gpio_output_bit(GpioPin::EthernetChipSelect, true);
	}

	/**
	 * Set bits in a KSZ8851 register.
	 */
	void register_set(RegisterOffset reg, uint16_t mask) const
	{
		uint16_t old = register_read(reg);
		register_write(reg, old | mask);
	}

	/**
	 * Clear bits in a KSZ8851 register.
	 */
	void register_clear(RegisterOffset reg, uint16_t mask) const
	{
		uint16_t old = register_read(reg);
		register_write(reg, old & ~mask);
	}

	/**
	 * Helper.  Returns a pointer to the SPI device.
	 */
	[[nodiscard, gnu::always_inline]] Capability<volatile SonataSpi> spi() const
	{
		return MMIO_CAPABILITY(SonataSpi, spi2);
	}

	/**
	 * Helper.  Returns a pointer to the GPIO device.
	 */
	[[nodiscard, gnu::always_inline]] Capability<volatile SonataGPIO>
	gpio() const
	{
		return MMIO_CAPABILITY(SonataGPIO, gpio);
	}

	/**
	 * Number of frames yet to be received since last interrupt acknowledgement.
	 */
	uint16_t framesToProcess = 0;

	/**
	 * Mutex protecting transmitBuffer if send_frame is reentered.
	 */
	RecursiveMutex transmitBufferMutex;

	/**
	 * Buffer used by send_frame.
	 */
	std::unique_ptr<uint8_t[]> transmitBuffer;

	/**
	 * Mutex protecting receiveBuffer if receive_frame is called before a
	 * previous returned frame is dropped.
	 */
	RecursiveMutex receiveBufferMutex;

	/**
	 * Reads and writes of the GPIO space use the same bits of the MMIO region
	 * and so need to be protected.
	 */
	FlagLockPriorityInherited gpioLock;

	/**
	 * Buffer used by receive_frame.
	 */
	std::unique_ptr<uint8_t[]> receiveBuffer;

	public:
	/**
	 * Initialise a reference to the Ethernet device.
	 */
	Ksz8851Ethernet()
	{
		transmitBuffer = std::make_unique<uint8_t[]>(MaxFrameSize);
		receiveBuffer  = std::make_unique<uint8_t[]>(MaxFrameSize);

		// Reset chip. It needs to be hold in reset for at least 10ms.
		set_gpio_output_bit(GpioPin::EthernetReset, false);
		thread_millisecond_wait(20);
		set_gpio_output_bit(GpioPin::EthernetReset, true);

		uint16_t chipId = register_read(RegisterOffset::ChipIdEnable);
		Debug::log("Chip ID is {}", chipId);

		// Check the chip ID. The last nibble is revision ID and can be ignored.
		Debug::Assert((chipId & 0xFFF0) == 0x8870, "Unexpected Chip ID");

		// This is the initialisation sequence suggested by the programmer's
		// guide.
		register_write(RegisterOffset::TransmitFrameDataPointer,
		               FrameDataPointer::FrameDataPointerAutoIncrement);
		register_write(RegisterOffset::TransmitControl,
		               TransmitControl::TransmitCrcEnable |
		                 TransmitControl::TransmitPaddingEnable |
		                 TransmitControl::TransmitFlowControlEnable |
		                 TransmitControl::TransmitChecksumGenerationIp |
		                 TransmitControl::TransmitChecksumGenerationTcp |
		                 TransmitControl::TransmitChecksumGenerationIcmp);
		register_write(RegisterOffset::ReceiveFrameDataPointer,
		               FrameDataPointer::FrameDataPointerAutoIncrement);
		// Configure Receive Frame Threshold for one frame.
		register_write(RegisterOffset::ReceiveFrameCountThreshold, 0x0001);
		register_write(RegisterOffset::ReceiveControl1,
		               ReceiveControl1::ReceiveUnicastEnable |
		                 ReceiveControl1::ReceiveMulticastEnable |
		                 ReceiveControl1::ReceiveBroadcastEnable |
		                 ReceiveControl1::ReceiveFlowControlEnable |
		                 ReceiveControl1::
		                   ReceivePhysicalAddressFilteringWithMacAddressEnable |
		                 ReceiveControl1::ReceiveIpFrameChecksumCheckEnable |
		                 ReceiveControl1::ReceiveTcpFrameChecksumCheckEnable |
		                 ReceiveControl1::ReceiveUdpFrameChecksumCheckEnable);
		// The frame data burst field in this register controls how many data
		// from a frame is read per DMA operation. The programmer's guide has a
		// 4 byte burst, but to reduce SPI transactions and improve performance
		// we choose to use single-frame data burst which reads the entire
		// Ethernet frame in a single SPI DMA.
		register_write(
		  RegisterOffset::ReceiveControl2,
		  ReceiveControl2::UdpLiteFrameEnable |
		    ReceiveControl2::ReceiveIpv4Ipv6UdpFrameChecksumEqualZero |
		    ReceiveControl2::ReceiveIpv4Ipv6FragmentFramePass |
		    ReceiveControl2::DataBurstSingleFrame);
		register_write(
		  RegisterOffset::ReceiveQueueCommand,
		  ReceiveQueueCommand::ReceiveFrameCountThresholdEnable |
		    ReceiveQueueCommand::AutoDequeueReceiveQueueFrameEnable);

		// Programmer's guide have a step to set the chip in half-duplex when
		// negotiation failed, but we omit the step since non-switching hubs and
		// half-duplex Ethernet is rarely used these days.

		register_set(RegisterOffset::Port1Control,
		             Port1Control::RestartAutoNegotiation);

		// Configure Low Watermark to 6KByte available buffer space out of
		// 12KByte (unit is 4 bytes).
		register_write(RegisterOffset::FlowControlLowWatermark, 0x0600);
		// Configure High Watermark to 4KByte available buffer space out of
		// 12KByte (unit is 4 bytes).
		register_write(RegisterOffset::FlowControlHighWatermark, 0x0400);

		// Clear the interrupt status
		register_write(RegisterOffset::InterruptStatus, 0xFFFF);
		receiveInterruptFutex =
		  interrupt_futex_get(STATIC_SEALED_VALUE(EthernetInterruptCapability));
		// Enable Receive interrupt
		register_write(RegisterOffset::InterruptEnable, ReceiveInterrupt);

		// Enable QMU Transmit.
		register_set(RegisterOffset::TransmitControl,
		             TransmitControl::TransmitEnable);
		// Enable QMU Receive.
		register_set(RegisterOffset::ReceiveControl1,
		             ReceiveControl1::ReceiveEnable);
	}

	Ksz8851Ethernet(const Ksz8851Ethernet &) = delete;
	Ksz8851Ethernet(Ksz8851Ethernet &&)      = delete;

	/**
	 * This device does not have a unique MAC address and so users must provide
	 * a locally administered MAC address if more than one device is present on
	 * the same network.
	 */
	static constexpr bool has_unique_mac_address()
	{
		return false;
	}

	static constexpr MACAddress mac_address_default()
	{
		return {0x3a, 0x30, 0x25, 0x24, 0xfe, 0x7a};
	}

	void mac_address_set(MACAddress address = mac_address_default())
	{
		register_write(RegisterOffset::MacAddressHigh,
		               (address[0] << 8) | address[1]);
		register_write(RegisterOffset::MacAdressMiddle,
		               (address[2] << 8) | address[3]);
		register_write(RegisterOffset::MacAddressLow,
		               (address[4] << 8) | address[5]);
	}

	uint32_t receive_interrupt_value()
	{
		return *receiveInterruptFutex;
	}

	int receive_interrupt_complete(Timeout *timeout,
	                               uint32_t lastInterruptValue)
	{
		// If there are frames to process, do not enter wait.
		if (framesToProcess)
		{
			return 0;
		}

		// Our interrupt is level-triggered; if a frame happens to arrive
		// between `receive_frame` call and we marking interrupt as received,
		// it will trigger again immediately after we acknowledge it.

		// Acknowledge the interrupt in the scheduler.
		interrupt_complete(STATIC_SEALED_VALUE(EthernetInterruptCapability));
		if (*receiveInterruptFutex == lastInterruptValue)
		{
			Debug::log("Acknowledged interrupt, sleeping on futex {}",
			           receiveInterruptFutex);
			return futex_timed_wait(
			  timeout, receiveInterruptFutex, lastInterruptValue);
		}
		Debug::log("Scheduler announces interrupt has fired");
		return 0;
	}

	/**
	 * Simple class representing a received Ethernet frame.
	 */
	class Frame
	{
		public:
		uint16_t            length;
		Capability<uint8_t> buffer;

		private:
		friend class Ksz8851Ethernet;
		LockGuard<RecursiveMutex> guard;

		Frame(LockGuard<RecursiveMutex> &&guard,
		      Capability<uint8_t>         buffer,
		      uint16_t                    length)
		  : guard(std::move(guard)), buffer(buffer), length(length)
		{
		}
	};

	/**
	 * Check the link status of the PHY.
	 */
	bool phy_link_status()
	{
		uint16_t status = register_read(RegisterOffset::Port1Status);
		return (status & Port1Status::LinkGood) != 0;
	}

	std::optional<Frame> receive_frame()
	{
		LockGuard g{gpioLock};
		if (framesToProcess == 0)
		{
			uint16_t isr = register_read(RegisterOffset::InterruptStatus);
			if (!(isr & ReceiveInterrupt))
			{
				return std::nullopt;
			}

			// Acknowledge the interrupt
			register_write(RegisterOffset::InterruptStatus, ReceiveInterrupt);

			// Read number of frames pending.
			// Note that this is only updated when we acknowledge the interrupt.
			framesToProcess =
			  register_read(RegisterOffset::ReceiveFrameCountThreshold) >> 8;
		}

		// Get number of frames pending
		for (; framesToProcess; framesToProcess--)
		{
			uint16_t status =
			  register_read(RegisterOffset::ReceiveFrameHeaderStatus);
			uint16_t length =
			  register_read(RegisterOffset::ReceiveFrameHeaderByteCount) &
			  0xFFF;
			bool valid =
			  (status & ReceiveFrameValid) &&
			  !(status &
			    (ReceiveCrcError | ReceiveRuntFrame | ReceiveFrameTooLong |
			     ReceiveMiiError | ReceiveUdpFrameChecksumStatus |
			     ReceiveTcpFrameChecksumStatus | ReceiveIpFrameChecksumStatus |
			     ReceiveIcmpFrameChecksumStatus));

			if (!valid)
			{
				DebugFrameDrops::log("Dropping frame with status: {}", status);

				drop_error_frame();
				continue;
			}

			if (length == 0)
			{
				DebugFrameDrops::log("Dropping frame with zero length");

				drop_error_frame();
				continue;
			}

			// The DMA transfer to the Ethernet MAC must be a multiple of 4
			// bytes.
			uint16_t paddedLength = (length + 3) & ~0x3;
			if (paddedLength > MaxFrameSize)
			{
				DebugFrameDrops::log("Dropping frame that is too large: {}",
				                     length);

				drop_error_frame();
				continue;
			}

			Debug::log("Receiving frame of length {}", length);

			LockGuard guard{receiveBufferMutex};

			// Reset receive frame pointer to zero and start DMA transfer
			// operation.
			register_write(RegisterOffset::ReceiveFrameDataPointer,
			               FrameDataPointer::FrameDataPointerAutoIncrement);
			register_set(RegisterOffset::ReceiveQueueCommand, StartDmaAccess);

			// Start receiving via SPI.
			uint8_t cmd = static_cast<uint8_t>(SpiCommand::ReadDma) << 6;
			set_gpio_output_bit(GpioPin::EthernetChipSelect, false);
			spi()->blocking_write(&cmd, 1);

			// Initial words are ReceiveFrameHeaderStatus and
			// ReceiveFrameHeaderByteCount which we have already know the value.
			uint8_t dummy[8];
			spi()->blocking_read(dummy, sizeof(dummy));

			spi()->blocking_read(receiveBuffer.get(), paddedLength);

			set_gpio_output_bit(GpioPin::EthernetChipSelect, true);

			register_clear(RegisterOffset::ReceiveQueueCommand, StartDmaAccess);
			framesToProcess -= 1;

			Capability<uint8_t> boundedBuffer{receiveBuffer.get()};
			boundedBuffer.bounds().set_inexact(length);
			// Remove all permissions except load.  This also removes global, so
			// that this cannot be captured.
			boundedBuffer.permissions() &=
			  CHERI::PermissionSet{CHERI::Permission::Load};

			return Frame{std::move(guard), boundedBuffer, length};
		}

		return std::nullopt;
	}

	/**
	 * Send a packet.  This will block if no buffer space is available on
	 * device.
	 *
	 * The third argument is a callback that allows the caller to check the
	 * frame before it's sent but after it's copied into memory that isn't
	 * shared with other compartments.
	 */
	bool send_frame(const uint8_t *buffer, uint16_t length, auto &&check)
	{
		// The DMA transfer to the Ethernet MAC must be a multiple of 4 bytes.
		uint16_t paddedLength = (length + 3) & ~0x3;
		if (paddedLength > MaxFrameSize)
		{
			Debug::log("Frame size {} is larger than the maximum size", length);
			return false;
		}

		LockGuard guard{transmitBufferMutex};

		// We must check the frame pointer and its length. Although it
		// is supplied by the firewall which is trusted, the firewall
		// does not check the pointer which is coming from external
		// untrusted components.
		Timeout t{10};
		if ((heap_claim_fast(&t, buffer) < 0) ||
		    (!CHERI::check_pointer<CHERI::PermissionSet{
		       CHERI::Permission::Load}>(buffer, length)))
		{
			return false;
		}

		memcpy(transmitBuffer.get(), buffer, length);
		if (!check(transmitBuffer.get(), length))
		{
			return false;
		}

		LockGuard g{gpioLock};

		// Wait for the transmit buffer to be available on the device side.
		// This needs to include the header.
		while ((register_read(RegisterOffset::TransmitQueueMemoryInfo) &
		        0xFFF) < length + 4)
		{
		}

		Debug::log("Sending frame of length {}", length);

		// Start DMA transfer operation.
		register_set(RegisterOffset::ReceiveQueueCommand, StartDmaAccess);

		// Start sending via SPI.
		uint8_t cmd = static_cast<uint8_t>(SpiCommand::WriteDma) << 6;
		set_gpio_output_bit(GpioPin::EthernetChipSelect, false);
		spi()->blocking_write(&cmd, 1);

		uint32_t header = static_cast<uint32_t>(length) << 16;
		spi()->blocking_write(reinterpret_cast<uint8_t *>(&header),
		                      sizeof(header));

		spi()->blocking_write(transmitBuffer.get(), paddedLength);

		spi()->wait_idle();
		set_gpio_output_bit(GpioPin::EthernetChipSelect, true);

		// Stop QMU DMA transfer operation.
		register_clear(RegisterOffset::ReceiveQueueCommand, StartDmaAccess);

		// Enqueue the frame for transmission.
		register_set(
		  RegisterOffset::TransmitQueueCommand,
		  TransmitQueueCommand::ManualEnqueueTransmitQueueFrameEnable);

		return true;
	}

	private:
	void drop_error_frame()
	{
		register_set(RegisterOffset::ReceiveQueueCommand,
		             ReleaseReceiveErrorFrame);
		// Wait for confirmation of frame release before attempting to process
		// next frame.
		while (register_read(RegisterOffset::ReceiveQueueCommand) &
		       ReleaseReceiveErrorFrame)
		{
		}
	}
};

using EthernetDevice = Ksz8851Ethernet;

static_assert(EthernetAdaptor<EthernetDevice>);

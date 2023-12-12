#pragma once
#include <array>
#include <cheri.hh>
#include <cstddef>
#include <cstdint>
#include <debug.hh>
#include <futex.h>
#include <interrupt.h>
#include <optional>
#include <platform/concepts/ethernet.hh>
#include <thread.h>
#include <type_traits>

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(EthernetReceive,
                                        EthernetReceiveInterrupt,
                                        true,
                                        true);

/**
 * The driver for Kunyan Liu's custom Ethernet MAC for the Arty A7.  This is
 * intended to run at 10Mb/s.  It provides two send and two receive buffers in
 * shared SRAM.
 *
 * WARNING: This is currently evolving and is not yet stable.
 */
class KunyanEthernet
{
	/**
	 * Flag set when we're debugging this driver.
	 */
	static constexpr bool DebugEthernet = false;

	/**
	 * Flag set to log messages when frames are dropped.
	 */
	static constexpr bool DebugDroppedFrames = false;

	/**
	 * Helper for conditional debug logs and assertions.
	 */
	using Debug = ConditionalDebug<DebugEthernet, "Ethernet driver">;

	/**
	 * Import the Capability helper from the CHERI namespace.
	 */
	template<typename T>
	using Capability = CHERI::Capability<T>;

	/**
	 * The location of registers
	 */
	enum class RegisterOffset : size_t
	{
		/**
		 * MAC control register.  Higher bits control filtering modes:
		 *
		 * 12: Drop frames with invalid CRC.
		 * 11: Allow incoming IPv6 multicast filtering
		 * 10: Allow incoming IPv4 multicast filtering
		 * 9: Allow incoming broadcast filtering
		 * 8: Allow incoming unicast filtering
		 *
		 *
		 * Write a 1 to reset the PHY.  This takes 167 ms.
		 */
		MACControl = 0,
		/**
		 * High 4 bytes of the MAC address.  Byte 2 is in bits 31:24, byte 5 is
		 * in 7:0.
		 */
		MACAddressHigh = 4,
		/**
		 * Low 2 bytes of the MAC address.  Byte 1 in bits 15:8, byte 0 in 7:0.
		 */
		MACAddressLow = 8,
		/**
		 * Scratch register, unused.
		 */
		Scratch = 12,
		/**
		 * MDIO address, controls which PHY register will be read or written.
		 */
		MDIOAddress = 0x10,
		/**
		 * MDIO Write, written to set the value to write to a PHY register.
		 */
		MDIODataWrite = 0x14,
		/**
		 * MDIO Read, set by the device to the value of a PHY register in
		 * response to an MDIO read.
		 */
		MDIODataRead = 0x18,
		/**
		 * MDIO control.  Used to control and report status of MDIO
		 * transactions.
		 */
		MDIOControl = 0x1c,
		/**
		 * Length of the frame written to the ping buffer for sending.
		 */
		TransmitFrameLengthPing = 0x20,
		/**
		 * Transmit control for the ping buffer.
		 */
		TransmitControlPing = 0x24,
		/**
		 * Length of the frame written to the pong buffer for sending.
		 */
		TransmitFrameLengthPong = 0x28,
		/**
		 * Transmit control for the pong buffer.
		 */
		TransmitControlPong = 0x2c,
		/**
		 * Receive control for the ping buffer.
		 */
		ReceiveControlPing = 0x30,
		/**
		 * Receive control for the pong buffer.
		 */
		ReceiveControlPong = 0x34,
		/**
		 * Interrupt enable.  Write 1 to enable, 0 to disable interrupts for
		 * each direction.  Bit 0 is transmit, bit 1 is receive.
		 */
		GlobalInterruptEnable = 0x38,
		/**
		 * Current interrupt status.  Bit 0 is transmit, bit 1 is receive.
		 *
		 * Write a 1 to each bit to clear.  The device will keep the interrupt
		 * asserted until it is explicitly cleared.
		 */
		InterruptStatus = 0x3c,
		/**
		 * Length of the frames in the ping and pong buffer, in the low and
		 * high 16 bits respectively.
		 */
		ReceiveFrameLength = 0x40,
		/**
		 * Saturating counter of the number of frames received.
		 */
		ReceivedFramesCount = 0x44,
		/**
		 * Saturating counter of the number of frames passed to software.
		 */
		ReceivedFramesPassedToSoftware = 0x48,
		/**
		 * Saturating counter of the number of frames dropped because the
		 * receive buffers were both full.
		 */
		ReceiveDroppedFramesBuffersFull = 0x4c,
		/**
		 * Saturating counter of the number of frames dropped because they had
		 * failed FCS checks.
		 */
		ReceiveDroppedFramesFCSFailed = 0x50,
		/**
		 * Saturating counter of the number of frames dropped because the
		 * target address was invalid.
		 */
		ReceiveDroppedFramesInvalidAddress = 0x54,
	};

	using MACAddress = std::array<uint8_t, 6>;

	/**
	 * MAC filtering modes.
	 */
	enum class FilterMode : uint32_t
	{
		/**
		 * Allow incoming frames without doing any address filtering.
		 */
		EnableAddressFiltering = 1 << 13,
		/**
		 * Drop frames with invalid CRC.
		 */
		DropInvalidCRC = 1 << 12,
		/**
		 * Allow incoming IPv6 multicast filtering
		 */
		AllowIPv6Multicast = 1 << 11,
		/**
		 * Allow incoming IPv4 multicast filtering
		 */
		AllowIPv4Multicast = 1 << 10,
		/**
		 * Allow incoming broadcast filtering
		 */
		AllowBroadcast = 1 << 9,
		/**
		 * Allow incoming unicast filtering
		 */
		AllowUnicast = 1 << 8,
	};

	/**
	 * The futex used to wait for interrupts when packets are available to
	 * receive.
	 */
	const uint32_t *receiveInterruptFutex;

	/**
	 * Counter for dropped frames, intended to be extensible to support
	 * different debugging registers.
	 */
	struct DroppedFrameCount
	{
		/**
		 * The old value of the counter.
		 */
		uint32_t droppedFrameCount[3];
		/**
		 * Get a reference to one of the recorded counters.
		 */
		template<RegisterOffset Reg>
		uint32_t &get();
		template<>
		uint32_t &get<RegisterOffset::ReceiveDroppedFramesBuffersFull>()
		{
			return droppedFrameCount[0];
		}

		template<>
		uint32_t &get<RegisterOffset::ReceiveDroppedFramesFCSFailed>()
		{
			return droppedFrameCount[1];
		}

		template<>
		uint32_t &get<RegisterOffset::ReceiveDroppedFramesInvalidAddress>()
		{
			return droppedFrameCount[2];
		}
	};

	/**
	 * Empty class, for use with `std::conditional_t` to conditionally add a
	 * field.
	 */
	struct Empty
	{
	};

	/**
	 * If we're building with debugging support for dropped-frame counters,
	 * reserve space for them.
	 */
	[[no_unique_address]] std::
	  conditional_t<DebugDroppedFrames, DroppedFrameCount, Empty>
	    droppedFrames;

	/**
	 * Log a message if the dropped-frame counter identified by `Reg` has
	 * changed.
	 */
	template<RegisterOffset Reg, typename T = decltype(droppedFrames)>
	void dropped_frames_log_if_changed(T &droppedFrames)
	{
		if constexpr (std::is_same_v<T, DroppedFrameCount>)
		{
			auto &lastCount = droppedFrames.template get<Reg>();
			auto  count     = mmio_register<Reg>();
			if (count != lastCount)
			{
				ConditionalDebug<true, "Ethernet driver">::log(
				  "Dropped frames in counter {}: {}", Reg, count);
				lastCount = count;
			}
		}
	}

	/**
	 * Set the state of one of the MAC filtering modes.  Returns the previous
	 * value of the mode.
	 */
	bool filter_mode_update(FilterMode mode, bool enable)
	{
		auto    &macControl = mmio_register<RegisterOffset::MACControl>();
		uint32_t modeBit    = static_cast<uint32_t>(mode);
		uint32_t oldMode    = macControl;
		Debug::log("Old filter mode {}: {}", mode, oldMode);
		if (enable)
		{
			macControl = oldMode | modeBit;
		}
		else
		{
			macControl = oldMode & ~modeBit;
		}
		return oldMode | modeBit;
	}

	/**
	 * The PHY registers are accessed through the MDIO interface.
	 */
	enum class PHYRegister
	{
		BasicControlRegister              = 0,
		BasicStatusRegister               = 1,
		Identifier1                       = 2,
		Identifier2                       = 3,
		AutoNegotiationAdvertisement      = 4,
		AutoNegotiationLinkPartnerAbility = 5,
	};

	enum BufferID
	{
		Ping = 0,
		Pong = 1,
	};

	bool receiveBufferInUse[2] = {false, false};
	bool sendBufferInUse[2]    = {false, false};

	BufferID nextReceiveBuffer = BufferID::Ping;

	constexpr static BufferID next_buffer_id(BufferID current)
	{
		return current == BufferID::Ping ? BufferID::Pong : BufferID::Ping;
	}

	/**
	 * Reset the PHY.  This must be done on initialisation.
	 */
	void phy_reset()
	{
		auto &macControl = mmio_register<RegisterOffset::MACControl>();
		macControl       = 1;
		// Wait for the PHY to reset.
		thread_millisecond_wait(167);
		// Initialise MDIO again after the PHY has been reset.
		mmio_register<RegisterOffset::MDIOControl>() = 0x10;
		// Wait to make sure MDIO initialisation has completed.
		thread_microsecond_spin(1);
	}

	/**
	 * Helper.  Returns a pointer to the device.
	 */
	[[nodiscard]] __always_inline Capability<volatile uint32_t>
	                              mmio_region() const
	{
		return MMIO_CAPABILITY(uint32_t, dmb);
	}

	/**
	 * Helper.  Returns a reference to a register in this device's MMIO region.
	 * The register is identified by a `RegisterOffset` value.
	 */
	template<RegisterOffset Offset>
	[[nodiscard]] volatile uint32_t &mmio_register() const
	{
		auto reg = mmio_region();
		reg.address() += static_cast<size_t>(Offset);
		reg.bounds() = sizeof(uint32_t);
		return *reg;
	}

	/**
	 * Poll the MDIO control register until a transaction is done.
	 *
	 * NOTE: This does no error handling.  It can infinite loop if the device
	 * is broken.  Normally, MDIO operations complete in fewer cycles than it
	 * takes to call this method and so it will return almost instantly.
	 */
	void mdio_wait_for_ready()
	{
		auto &mdioControl = mmio_register<RegisterOffset::MDIOControl>();
		while (mdioControl & 1) {}
	}

	/**
	 * Start an MDIO transaction.  This assumes that the MDIO control register
	 * and, for write operations, the MDIO data write register have been set up.
	 * A call to `mdio_wait_for_ready` is required for read transactions to
	 * ensure that the operation has completed before reading the MIDO data read
	 * register.
	 */
	void mdio_start_transaction()
	{
		auto &mdioControl = mmio_register<RegisterOffset::MDIOControl>();
		Debug::Assert(((mdioControl & ((1 << 3) | 1)) == 0), "MDIO is busy");
		// Write the MDIO enable bit and the start bit.
		mdioControl = (1 << 3) | 1;
	}

	/**
	 * Returns a pointer to one of the receive buffers, identified by the buffer
	 * identifier (ping or pong).
	 */
	uint8_t *receive_buffer_pointer(BufferID index = BufferID::Ping)
	{
		auto buffer = mmio_region();
		buffer.address() +=
		  static_cast<size_t>(index == BufferID::Ping ? 0x2000 : 0x2800);
		buffer.bounds() = 0x800;
		return const_cast<uint8_t *>(
		  reinterpret_cast<volatile uint8_t *>((buffer.get())));
	}

	/**
	 * Returns a pointer to one of the receive buffers, identified by the buffer
	 * identifier (ping or pong).
	 */
	uint8_t *transmit_buffer_pointer(BufferID index = BufferID::Ping)
	{
		auto buffer = mmio_region();
		buffer.address() +=
		  static_cast<size_t>(index == BufferID::Ping ? 0x1000 : 0x1800);
		buffer.bounds() = 0x800;
		return const_cast<uint8_t *>(
		  reinterpret_cast<volatile uint8_t *>((buffer.get())));
	}

	/**
	 * Write a value to a PHY register via MDIO.
	 */
	void
	mdio_write(uint8_t phyAddress, PHYRegister registerAddress, uint16_t data)
	{
		mdio_wait_for_ready();
		auto    &mdioAddress = mmio_register<RegisterOffset::MDIOAddress>();
		auto    &mdioWrite   = mmio_register<RegisterOffset::MDIODataWrite>();
		uint32_t writeCommand =
		  (0 << 10) | (phyAddress << 5) | uint32_t(registerAddress);
		mdioAddress = writeCommand;
		mdioWrite   = data;
		mdio_start_transaction();
	}

	/**
	 * Read a value from a PHY register via MDIO.
	 */
	uint16_t mdio_read(uint8_t phyAddress, PHYRegister registerAddress)
	{
		mdio_wait_for_ready();
		auto    &mdioAddress = mmio_register<RegisterOffset::MDIOAddress>();
		uint32_t readCommand =
		  (1 << 10) | (phyAddress << 5) | uint8_t(registerAddress);
		mdioAddress = readCommand;
		mdio_start_transaction();
		mdio_wait_for_ready();
		return mmio_register<RegisterOffset::MDIODataRead>();
	}

	public:
	/**
	 * Initialise a reference to the Ethernet device.  This will check the ID
	 * registers to make sure that this is the kind of device that we're
	 * expecting.
	 */
	KunyanEthernet()
	{
		phy_reset();
		// Check that this is the device that we're looking for
		Debug::Assert(
		  [&]() { return mdio_read(1, PHYRegister::Identifier1) == 0x2000; },
		  "PHY identifier 1 is not 0x2000");
		Debug::Assert(
		  [&]() {
			  return (mdio_read(1, PHYRegister::Identifier2) & 0xFFF0) ==
			         0x5c90;
		  },
		  "PHY identifier 1 is not 0x5c9x");
		autonegotiation_enable();
		filter_mode_update(FilterMode::DropInvalidCRC, true);
		filter_mode_update(FilterMode::EnableAddressFiltering, false);
		filter_mode_update(FilterMode::AllowUnicast, false);
		filter_mode_update(FilterMode::AllowIPv4Multicast, false);
		filter_mode_update(FilterMode::AllowIPv6Multicast, false);
		receiveInterruptFutex =
		  interrupt_futex_get(STATIC_SEALED_VALUE(EthernetReceive));
		// Enable receive interrupts
		mmio_register<RegisterOffset::GlobalInterruptEnable>() = 0b10;
		// Clear pending receive interrupts.
		mmio_register<RegisterOffset::InterruptStatus>() = 0b10;
	}

	KunyanEthernet(const KunyanEthernet &) = delete;
	KunyanEthernet(KunyanEthernet &&)      = delete;

	static constexpr MACAddress mac_address_default()
	{
		// FIXME: Generate a random locally administered MAC for each device.
		return {0x60, 0x6A, 0x6A, 0x37, 0x47, 0x88};
	}

	void mac_address_set(MACAddress address = mac_address_default())
	{
		auto &macAddressHigh = mmio_register<RegisterOffset::MACAddressHigh>();
		auto &macAddressLow  = mmio_register<RegisterOffset::MACAddressLow>();
		macAddressHigh       = (address[2] << 24) | (address[3] << 16) |
		                 (address[4] << 8) | address[5];
		macAddressLow = (address[1] << 8) | address[0];
	}

	uint32_t receive_interrupt_value()
	{
		return *receiveInterruptFutex;
	}

	int receive_interrupt_complete(Timeout *timeout,
	                               uint32_t lastInterruptValue)
	{
		// Clear the interrupt on the device.
		auto &interruptStatus =
		  mmio_register<RegisterOffset::InterruptStatus>();
		interruptStatus = 0b10;
		// There's a small window in between finishing checking the receive
		// buffers and clearing the interrupt where we could miss the
		// interrupt.  Check that the receive buffers are empty *after*
		// reenabling the interrupt to ensure that we don't sleep in this
		// period.
		if (check_frame(BufferID::Ping) || check_frame(BufferID::Pong))
		{
			Debug::log("Packets already ready, not waiting for interrupt");
			return 0;
		}
		// Acknowledge the interrupt in the scheduler.
		interrupt_complete(STATIC_SEALED_VALUE(EthernetReceive));
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
	 * Enable autonegotiation on the PHY.
	 *
	 * FIXME: This blocks forever if the cable is disconnected!
	 */
	void autonegotiation_enable(uint8_t phyAddress = 1)
	{
		Debug::log("Starting autonegotiation");
		// Advertise 802.3, 10Base-T full and half duplex
		mdio_write(phyAddress,
		           PHYRegister::AutoNegotiationAdvertisement,
		           (1 << 5) | (1 << 6) | 1);
		// Enable and restart autonegitiation
		mdio_write(
		  phyAddress, PHYRegister::BasicControlRegister, (1 << 12) | (1 << 9));
		Debug::log("Waiting for autonegotiation to complete");
		uint32_t loops = 0;
		while ((mdio_read(phyAddress, PHYRegister::BasicStatusRegister) &
		        (1 << 5)) == 0)
		{
			if ((loops++ & 0xfffff) == 0)
			{
				Debug::log(
				  "Waiting for autonegotiation to complete ({} loops): status: "
				  "{}",
				  loops,
				  mdio_read(phyAddress, PHYRegister::BasicStatusRegister));
			}
			yield();
		}
		Debug::Assert(
		  [&]() {
			  return (mdio_read(phyAddress, PHYRegister::BasicStatusRegister) &
			          (1 << 2)) != 0;
		  },
		  "Autonegotiation completed but link is down");
		Debug::log("Autonegotiation complete, status: {}",
		           mdio_read(phyAddress, PHYRegister::BasicStatusRegister));
		// Write 1 to clear the receive status.
		mmio_register<RegisterOffset::ReceiveControlPing>() = 1;
		mmio_register<RegisterOffset::ReceiveControlPong>() = 1;
	}

	/**
	 * Check the link status of the PHY.
	 */
	bool phy_link_status()
	{
		return (mdio_read(1, PHYRegister::BasicStatusRegister) & (1 << 2)) != 0;
	}

	std::optional<uint16_t> check_frame(BufferID index = BufferID::Ping)
	{
		// Debug::log("Checking for frame in buffer {}", index);
		auto receiveControl =
		  index == BufferID::Ping
		    ? mmio_register<RegisterOffset::ReceiveControlPing>()
		    : mmio_register<RegisterOffset::ReceiveControlPong>();
		if ((receiveControl & 1) == 0)
		{
			return std::nullopt;
		}
		Debug::log("Buffer has a frame.  Error? {}", receiveControl & 2);
		Debug::log("Buffer length: {}", receiveControl >> 16);
		return {receiveControl >> 16};
	}

	void complete_receive(BufferID index = BufferID::Ping)
	{
		Debug::log("Completing receive in buffer {}", index);
		// This buffer is now free to receive another frame.
		receiveBufferInUse[index] = false;
		if (index == BufferID::Ping)
		{
			mmio_register<RegisterOffset::ReceiveControlPing>() = 1;
		}
		else
		{
			mmio_register<RegisterOffset::ReceiveControlPong>() = 1;
		}
	}

	/**
	 * Class encapsulating a frame that has been received.  This holds a
	 * reference to the internal state of the buffer and will mark the buffer
	 * as free when it is destroyed.
	 */
	class BufferedFrame
	{
		friend class KunyanEthernet;
		KunyanEthernet &ethernetAdaptor;
		BufferID        owningBuffer;

		private:
		BufferedFrame(KunyanEthernet     &ethernetAdaptor,
		              BufferID            owningBuffer,
		              Capability<uint8_t> buffer,
		              uint16_t            length)

		  : ethernetAdaptor(ethernetAdaptor),
		    owningBuffer(owningBuffer),
		    buffer(buffer),
		    length(length)
		{
			// Mark this buffer as in use and try to advance the next buffer
			ethernetAdaptor.receiveBufferInUse[owningBuffer] = true;
			if (!ethernetAdaptor.receiveBufferInUse[next_buffer_id(
			      ethernetAdaptor.nextReceiveBuffer)])
			{
				ethernetAdaptor.nextReceiveBuffer =
				  next_buffer_id(ethernetAdaptor.nextReceiveBuffer);
			}
		}

		public:
		uint16_t            length;
		Capability<uint8_t> buffer;
		BufferedFrame(const BufferedFrame &) = delete;
		BufferedFrame(BufferedFrame &&other)
		  : ethernetAdaptor(other.ethernetAdaptor),
		    owningBuffer(other.owningBuffer),
		    buffer(other.buffer),
		    length(other.length)
		{
			other.buffer = nullptr;
			other.length = 0;
		}
		~BufferedFrame()
		{
			if (buffer != nullptr)
			{
				ethernetAdaptor.complete_receive(owningBuffer);
			}
		}
	};

	std::optional<BufferedFrame> receive_frame()
	{
		if (receiveBufferInUse[nextReceiveBuffer])
		{
			if (!receiveBufferInUse[next_buffer_id(nextReceiveBuffer)])
			{
				nextReceiveBuffer = next_buffer_id(nextReceiveBuffer);
			}
			else
			{
				return std::nullopt;
			}
		}
		auto maybeLength = check_frame(nextReceiveBuffer);
		if (!maybeLength)
		{
			return std::nullopt;
		}
		auto length = *maybeLength;
		// Strip the FCS from the length.
		length -= 4;
		auto                buffer = receive_buffer_pointer(nextReceiveBuffer);
		Capability<uint8_t> boundedBuffer{buffer};
		boundedBuffer.bounds() = length;
		// Remove all permissions except load.  This also removes global, so
		// that this cannot be captured.
		boundedBuffer.permissions() &=
		  CHERI::PermissionSet{CHERI::Permission::Load};
		Debug::log("Received frame from buffer {}", nextReceiveBuffer);
		return {{*this, nextReceiveBuffer, buffer, length}};
	}

	/**
	 * Send a packet.  This is synchronous and will block until the packet has
	 * been sent.  A better version would use the next available ping or pong
	 * buffer and return as soon as the send operation had been enqueued.
	 *
	 * The third argument is a callback that allows the caller to check the
	 * frame before it's sent but after it's copied into memory that isn't
	 * shared with other compartments.
	 */
	bool send_frame(const uint8_t *buffer, uint16_t length, auto &&check)
	{
		auto &transmitControl =
		  mmio_register<RegisterOffset::TransmitControlPing>();
		// Spin waiting for the transmit buffer to be free.
		while (transmitControl & 1) {}
		// Write the frame to the transmit buffer.
		auto transmitBuffer = transmit_buffer_pointer();
		memcpy(transmitBuffer, buffer, length);
		if (!check(transmitBuffer, length))
		{
			return false;
		}
		// The Ethernet standard requires frames to be at least 60 bytes long.
		// If we're asked to send anything shorter, pad it with zeroes.
		// (It would be nice if the MAC did this automatically).
		if (length < 60)
		{
			memset(transmitBuffer + length, 0, 60 - length);
			length = 60;
		}
		// Write the length of the frame to the transmit length register.
		mmio_register<RegisterOffset::TransmitFrameLengthPing>() = length;
		// Start the transmit.
		transmitControl = 1;
		Debug::log("Sent frame, waiting for completion");
		while (transmitControl & 1) {}
		// Return if the frame was sent successfully.
		Debug::log("Transmit control register: {}", transmitControl);
		if ((transmitControl & 2) != 0)
		{
			Debug::log("Error sending frame");
		}
		return (transmitControl & 2) == 0;
	}

	/**
	 * If debugging dropped frames is enabled, log any counter values that have
	 * changed since the last call to this function.
	 */
	void dropped_frames_log_all_if_changed()
	{
		if constexpr (DebugDroppedFrames)
		{
			dropped_frames_log_if_changed<
			  RegisterOffset::ReceiveDroppedFramesBuffersFull>(droppedFrames);
			dropped_frames_log_if_changed<
			  RegisterOffset::ReceiveDroppedFramesFCSFailed>(droppedFrames);
			dropped_frames_log_if_changed<
			  RegisterOffset::ReceiveDroppedFramesInvalidAddress>(
			  droppedFrames);
		}
	}

	void received_frames_log()
	{
		auto count = mmio_register<RegisterOffset::ReceivedFramesCount>();
		ConditionalDebug<true, "Ethernet driver">::log("Received frames: {}",
		                                               count);
	}
};

using EthernetDevice = KunyanEthernet;

static_assert(EthernetAdaptor<EthernetDevice>);

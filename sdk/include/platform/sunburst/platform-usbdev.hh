// SPDX-FileCopyrightText: CHERIoT contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include <cdefs.h>
#include <optional>
#include <stdint.h>
#include <utils.hh>

/**
 * A driver for OpenTitan USB Device, which is used in the Sonata system.
 *
 * This peripheral's source and documentation can be found at:
 * https://github.com/lowRISC/opentitan/tree/ab878b5d3578939a04db72d4ed966a56a869b2ed/hw/ip/usbdev
 *
 * With rendered register documentation served at:
 * https://opentitan.org/book/hw/ip/usbdev/doc/registers.html
 *
 * An incredibly brief overview of how the USB device and it's buffers:
 * Data packet ingress and egress goes via a pool of 64 byte buffers living
 * in a 2kB SRAM packet buffer, which is accessable as a large MMIO region.
 * The software manages these buffers using buffer IDs as references.
 * IDs are pushed or popped into different FIFOs by either the software or
 * device depending on whether they contain a packet to be sent, are available
 * for a packet to be received into them, or contain a packet that has been
 * received.
 *
 * See https://opentitan.org/book/hw/ip/usbdev/doc/programmers_guide.html for
 * more information.
 */
class OpenTitanUsbdev : private utils::NoCopyNoMove
{
	public:
	/// Supported sizes for the USB Device.
	static constexpr uint8_t MaxPacketLength = 64u;
	static constexpr uint8_t BufferCount     = 32u;
	static constexpr uint8_t MaxEndpoints    = 12u;

	/**
	 * The offset from the start of the USB Device MMIO region at which
	 * packet buffer memory begins.
	 */
	static constexpr uint32_t BufferStartAddress = 0x800u;

	/// Device Registers
	uint32_t interruptState;
	uint32_t interruptEnable;
	uint32_t interruptTest;
	uint32_t alertTest;
	uint32_t usbControl;
	uint32_t endpointOutEnable;
	uint32_t endpointInEnable;
	uint32_t usbStatus;
	uint32_t availableOutBuffer;
	uint32_t availableSetupBuffer;
	uint32_t receiveBuffer;
	/// Register to enable receive SETUP transactions
	uint32_t receiveEnableSetup;
	/// Register to enable receive OUT transactions
	uint32_t receiveEnableOut;
	/// Register to set NAK (Not/Negated Acknowledge) after OUT transactions
	uint32_t setNotAcknowledgeOut;
	/// Register showing ACK receival to indicate a successful IN send
	uint32_t inSent;
	/// Registers for controlling the stalling of OUT and IN endpoints
	uint32_t outStall;
	uint32_t inStall;
	/**
	 * IN transaction configuration registers. There is one register per
	 * endpoint for the USB device.
	 */
	uint32_t configIn[MaxEndpoints];
	/**
	 * Registers for configuring which endpoints should be treated as
	 * isochronous endpoints. This means that if the corresponding bit is set,
	 * then that no handshake packet will be sent for an OUT/IN transaction on
	 * that endpoint.
	 */
	uint32_t outIsochronous;
	uint32_t inIsochronous;
	/// Registers for configuring if endpoints data toggle on transactions
	uint32_t outDataToggle;
	uint32_t inDataToggle;

	private:
	/**
	 * Registers to sense/drive the USB PHY pins. That is, these registers can
	 * be used to respectively read out the state of the USB device inputs and
	 * outputs, or to control the inputs and outputs from software. These
	 * registers are kept private as they are intended to be used for debugging
	 * purposes or during chip testing, and not in actual software.
	 */
	[[maybe_unused]] uint32_t phyPinsSense;
	[[maybe_unused]] uint32_t phyPinsDrive;

	public:
	/// Config register for the USB PHY pins.
	uint32_t phyConfig;

	/// Interrupt definitions for OpenTitan's USB Device.
	enum class UsbdevInterrupt : uint32_t
	{
		/// Interrupt asserted whilst the receive FIFO (buffer) is not empty.
		PacketReceived = 1u << 0,
		/**
		 * Interrupt asserted when a packet was sent as part of an IN
		 * transaction, but not cleared from the `inSent` register.
		 */
		PacketSent = 1u << 1,
		/**
		 * Interrupt raised when VBUS (power supply) is lost, i.e. the link to
		 * the USB host controller has been disconnected.
		 */
		Disconnected = 1u << 2,
		/**
		 * Interrupt raised when the link is active, but a Start of Frame (SOF)
		 * packet has not been received within a given timeout threshold, which
		 * is set to 4.096 milliseconds.
		 */
		HostLost = 1u << 3,
		/**
		 * Interrupt raised when a Bus Reset condition is indicated on the link
		 * by the link being held in an SE0 state (Single Ended Zero, both lines
		 * being pulled low) for longer than 3 microseconds.
		 */
		LinkReset = 1u << 4,
		/**
		 * Interrupt raised when the link has entered the suspend state, due to
		 * being idle for more than 3 milliseconds.
		 */
		LinkSuspend = 1u << 5,
		///  Interrupt raised on link transition from suspended to non-idle.
		LinkResume = 1u << 6,
		/// Interrupt asserted whilst the Available OUT buffer is empty.
		AvailableOutEmpty = 1u << 7,
		///  Interrupt asserted whilst the Receive buffer is full.
		ReceiveFull = 1u << 8,
		/**
		 * Interrupt raised when the Available OUT buffer or the Available SETUP
		 * buffer overflows.
		 */
		AvailableBufferOverflow = 1u << 9,
		///  Interrupt raised when an error occurs during an IN transaction.
		LinkInError = 1u << 10,
		/**
		 * Interrupt raised when a CRC (cyclic redundancy check) error occurs on
		 * a received packet; i.e. there was an error in transmission.
		 */
		RedundancyCheckError = 1u << 11,
		///  Interrupt raised when an invalid Packet Identifier is received.
		PacketIdentifierError = 1u << 12,
		///  Interrupt raised when a bit stuffing violation is detected.
		BitstuffingError = 1u << 13,
		/**
		 * Interrupt raised when the USB frame number is updated with a valid
		 * SOF (Start of Frame) packet.
		 */
		FrameUpdated = 1u << 14,
		///  Interrupt raised when VBUS (power supply) is detected.
		Powered = 1u << 15,
		///  Interrupt raised when an error occurs during an OUT transaction.
		LinkOutError = 1u << 16,
		///  Interrupt asserted whilst the Available SETUP buffer is empty.
		AvailableSetupEmpty = 1u << 17,
	};

	/**
	 * Definitions of fields (and their locations) for the USB Control register
	 * (offset 0x10).
	 *
	 * https://opentitan.org/book/hw/ip/usbdev/doc/registers.html#usbctrl
	 */
	enum class UsbControlField : uint32_t
	{
		Enable           = 1u << 0,
		ResumeLinkActive = 1u << 1,
		DeviceAddress    = 0x7Fu << 16,
	};

	/**
	 * Definitions of fields (and their locations) for the USB Status register
	 * (offset 0x1c).
	 *
	 * https://opentitan.org/book/hw/ip/usbdev/doc/registers.html#usbstat
	 */
	enum class UsbStatusField : uint32_t
	{
		Frame               = 0x7FFu << 0,
		HostLost            = 1u << 11,
		LinkState           = 0x7u << 12,
		Sense               = 1u << 15,
		AvailableOutDepth   = 0xFu << 16,
		AvailableSetupDepth = 0x7u << 20,
		AvailableOutFull    = 1u << 23,
		ReceiveDepth        = 0xFu << 24,
		AvailableSetupFull  = 1u << 30,
		ReceiveEmpty        = 1u << 31,
	};

	/**
	 * Definitions of fields (and their locations) for the Receive FIFO
	 * buffer register (offset 0x28).
	 *
	 * https://opentitan.org/book/hw/ip/usbdev/doc/registers.html#rxfifo
	 */
	enum class ReceiveBufferField : uint32_t
	{
		BufferId   = 0x1Fu << 0,
		Size       = 0x7Fu << 8,
		Setup      = 1u << 19,
		EndpointId = 0xFu << 20,
	};

	/**
	 * Definitions of fields (and their locations) for a Config In register
	 * (where there is one such register for each endpoint). These are
	 * the registers with offsets 0x44 up to (and not including) 0x74.
	 *
	 * https://opentitan.org/book/hw/ip/usbdev/doc/registers.html#configin
	 */
	enum class ConfigInField : uint32_t
	{
		BufferId = 0x1Fu << 0,
		Size     = 0x7Fu << 8,
		Sending  = 1u << 29,
		Pending  = 1u << 30,
		Ready    = 1u << 31,
	};

	/**
	 * Definitions of fields (and their locations) for the PHY Config
	 * Register (offset 0x8c).
	 *
	 * https://opentitan.org/book/hw/ip/usbdev/doc/registers.html#phy_config
	 */
	enum class PhyConfigField : uint32_t
	{
		UseDifferentialReceiver = 1u << 0,
		//  Other PHY Configuration fields are omitted.
	};

	/**
	 * Ensure that the Available OUT and Available SETUP buffers are kept
	 * supplied with buffers for packet reception.
	 *
	 * @param bufferBitmap A bitmap of the buffers that are not currently
	 * committed (where 1 corresponds to not in use).
	 * @returns The updated bitmap after supplying buffers.
	 */
	[[nodiscard]] uint64_t supply_buffers(uint64_t bufferBitmap) volatile
	{
		constexpr uint32_t SetupFullBit =
		  uint32_t(UsbStatusField::AvailableSetupFull);
		constexpr uint32_t OutFullBit =
		  uint32_t(UsbStatusField::AvailableOutFull);

		for (uint8_t index = 0; index < BufferCount; index++)
		{
			const uint32_t Buffer = (1u << index);
			if (!(bufferBitmap & Buffer))
			{
				continue; // Skip buffers that are not available
			}

			// If a buffer is available, and either Available SETUP or OUT are
			// not yet full, then commit that buffer and mark it as in use.
			if (usbStatus & SetupFullBit)
			{
				if (usbStatus & OutFullBit)
				{
					break; // Both are full - stop trying to supply buffers.
				}
				availableOutBuffer = index;
			}
			else
			{
				availableSetupBuffer = index;
			}
			bufferBitmap &= ~Buffer;
		}
		return bufferBitmap;
	}

	/**
	 * Enable a specified interrupt / interrupts.
	 */
	void interrupt_enable(UsbdevInterrupt interrupt) volatile
	{
		interruptEnable = interruptEnable | uint32_t(interrupt);
	}

	/**
	 * Disable a specified interrupt / interrupts.
	 */
	void interrupt_disable(UsbdevInterrupt interrupt) volatile
	{
		interruptEnable = interruptEnable & ~uint32_t(interrupt);
	}

	/**
	 * Initialise the USB device, ensuring that packet buffers are available for
	 * reception and that the PHY has been appropriately configured. Note that
	 * at this stage, endpoints have not been configured and the device has not
	 * been connected to the USB.
	 *
	 * @param bufferBitmap An out-parameter, to initialise a bitmap of the
	 * buffers that are not currently commited (1 corresponds to not in use).
	 *
	 * @returns 0 if initialisation is sucessful, and non-zero otherwise.
	 */
	[[nodiscard]] int init(uint64_t &bufferBitmap) volatile
	{
		bufferBitmap = supply_buffers((uint64_t(1u) << BufferCount) - 1u);
		phyConfig    = uint32_t(PhyConfigField::UseDifferentialReceiver);
		return 0;
	}

	/**
	 * Set up the configuration of an OUT endpoint for the USB device.
	 *
	 * @param endpointId The ID of the OUT endpoint to configure.
	 * @param enabled Whether the OUT endpoint should be enabled or not.
	 * @param setup Whether SETUP transactions should be enabled for the
	 * endpoint.
	 * @param isochronous Whether the endpoint should operate isochronously or
	 * non-isochronously.
	 *
	 * @returns 0 if configuration is successful, and non-zero otherwise.
	 */
	[[nodiscard]] int out_endpoint_configure(uint8_t endpointId,
	                                         bool    enabled,
	                                         bool    setup,
	                                         bool    isochronous) volatile
	{
		if (endpointId >= MaxEndpoints)
		{
			return -1;
		}
		const uint32_t Mask = 1u << endpointId;
		endpointOutEnable = (endpointOutEnable & ~Mask) | (enabled ? Mask : 0u);
		outIsochronous = (outIsochronous & ~Mask) | (isochronous ? Mask : 0u);
		receiveEnableSetup = (receiveEnableSetup & ~Mask) | (setup ? Mask : 0u);
		receiveEnableOut   = (receiveEnableOut & ~Mask) | (enabled ? Mask : 0u);
		return 0;
	}

	/**
	 * Set up the configuration of an IN endpoint for the USB device.
	 *
	 * @param endpointId The ID of the IN endpoint to configure
	 * @param enabled Whether the IN endpoint should be enabled or not.
	 * @param isochronous Whether the endpoint should operate isochronously or
	 * non-isochronously.
	 *
	 * @returns 0 if configuration is successful, and non-zero otherwise.
	 */
	[[nodiscard]] int in_endpoint_configure(uint8_t endpointId,
	                                        bool    enabled,
	                                        bool    isochronous) volatile
	{
		if (endpointId >= MaxEndpoints)
		{
			return -1;
		}
		const uint32_t Mask = 1u << endpointId;
		endpointInEnable = (endpointInEnable & ~Mask) | (enabled ? Mask : 0u);
		inIsochronous    = (inIsochronous & ~Mask) | (isochronous ? Mask : 0u);
		return 0;
	}

	/**
	 * Set the STALL state of a specified endpoint pair (both IN and OUT).
	 *
	 * @param endpointId The ID of the endpoint pair to modify.
	 * @param stalling Whether the endpoints are stalling or not.
	 *
	 * @returns 0 if successful, and non-zero otherwise.
	 */
	[[nodiscard]] int endpoint_stalling_set(uint8_t endpointId,
	                                        bool    stalling) volatile
	{
		if (endpointId >= MaxEndpoints)
		{
			return -1;
		}
		const uint32_t Mask = 1u << endpointId;
		outStall            = (outStall & ~Mask) | (stalling ? Mask : 0u);
		inStall             = (inStall & ~Mask) | (stalling ? Mask : 0u);
		return 0;
	}

	/**
	 * Connect the device to the USB, indicating its presence to the USB host
	 * controller. Endpoints must already have been configured at this point
	 * because traffic may be received imminently.
	 *
	 * @returns 0 if successful, and non-zero otherwise.
	 * @returns -1 if endpoint 0 isn't enabled,
	 *             suggesting the endpoints haven't been configured.
	 */
	[[nodiscard]] int connect() volatile
	{
		if (!(endpointInEnable & endpointOutEnable & 0b1))
		{
			return -1;
		}
		usbControl = usbControl | uint32_t(UsbControlField::Enable);
		return 0;
	}

	/**
	 * Disconnect the device from the USB.
	 *
	 * @returns 0 if successful, and non-zero otherwise.
	 */
	void disconnect() volatile
	{
		usbControl = usbControl & ~uint32_t(UsbControlField::Enable);
	}

	/**
	 * Check whether the USB device is connected (i.e. pullup enabled).
	 *
	 * @returns True to indicate it is connected, and false otherwise.
	 */
	[[nodiscard]] bool connected() volatile
	{
		return (usbControl & uint32_t(UsbControlField::Enable));
	}

	/**
	 * Set the device address on the USB; this address will have been supplied
	 * by the USB host controller in the standard `SET_ADDRESS` Control
	 * Transfer.
	 *
	 * @param address The device address to set on the USB.
	 *
	 * @returns 0 if successful, and non-zero otherwise.
	 */
	[[nodiscard]] int device_address_set(uint8_t address) volatile
	{
		if (address >= 0x80)
		{
			return -1; // Device addresses are only 7 bits long.
		}
		constexpr uint32_t Mask = uint32_t(UsbControlField::DeviceAddress);
		usbControl              = (usbControl & ~Mask) | (address << 16);
		return 0;
	}

	/**
	 * Check and retrieve the endpoint and buffer numbers of a
	 * recently-collected IN data packet. The caller is responsible for reusing
	 * or releasing the buffer.
	 *
	 * @param endpointId An out-parameter, to which the ID of the endpoint for
	 * a recently-collected IN data packet will be written.
	 * @param bufferId An out-parameter, to which the ID of the buffer for a
	 * recently-collected IN data packet will be written.
	 *
	 * @returns 0 if successful, and non-zero otherwise.
	 */
	[[nodiscard]] int retrieve_collected_packet(uint8_t &endpointId,
	                                            uint8_t &bufferId) volatile
	{
		constexpr uint32_t BufferIdMask = uint32_t(ConfigInField::BufferId);
		uint32_t           sent         = inSent;

		// Clear the first encountered packet sent indication.
		for (endpointId = 0; endpointId < MaxEndpoints; endpointId++)
		{
			const uint32_t EndpointBit = 1u << endpointId;
			if (sent & EndpointBit)
			{
				// Clear the `in_sent` bit for this specific endpoint, and
				// indicate which buffer has been released.
				inSent   = EndpointBit;
				bufferId = (configIn[endpointId] & BufferIdMask);
				return 0;
			}
		}

		// If no packet sent indications were found, then fail.
		return -1;
	}

	/**
	 * Present a packet on the specified IN endpoint for collection by the USB
	 * host controller.
	 *
	 * @param bufferId The buffer to use to store the packet.
	 * @param endpointId The IN endpoint used to send the packet.
	 * @param data The packet to be transmitted.
	 * @param size The size of the packet.
	 */
	void packet_send(uint8_t         bufferId,
	                 uint8_t         endpointId,
	                 const uint32_t *data,
	                 uint8_t         size) volatile
	{
		// Transmission of zero length packets is common over USB
		if (size > 0)
		{
			usbdev_transfer(buffer(bufferId), data, size, true);
		}

		constexpr uint32_t ReadyBit = uint32_t(ConfigInField::Ready);
		configIn[endpointId]        = bufferId | (size << 8);
		configIn[endpointId]        = configIn[endpointId] | ReadyBit;
	}

	/// The information associated with a received packet
	struct ReceiveBufferInfo
	{
		uint32_t info;
		/// The endpoint ID the received packet was received on
		constexpr uint8_t endpoint_id()
		{
			return (info & uint32_t(ReceiveBufferField::EndpointId)) >> 20;
		}
		/// The size of the received packet
		constexpr uint16_t size()
		{
			return (info & uint32_t(ReceiveBufferField::Size)) >> 8;
		}
		/// Whether the received packet was a setup packet
		constexpr bool is_setup()
		{
			return (info & uint32_t(ReceiveBufferField::Setup)) != 0;
		}
		/// The buffer ID used to store the received packet
		constexpr uint8_t buffer_id()
		{
			return (info & uint32_t(ReceiveBufferField::BufferId)) >> 0;
		}
	};

	/**
	 * If a packet has been received, removes the packet's buffer from the
	 * receive FIFO giving it's information and ownership to the user.
	 *
	 * `packet_data_get` can be used to retrieve the packet's data.
	 *
	 * @returns Information about the received packet, if a packet had been
	 * received.
	 */
	[[nodiscard]] std::optional<ReceiveBufferInfo> packet_take() volatile
	{
		if (!(usbStatus & uint32_t(UsbStatusField::ReceiveDepth)))
		{
			return {}; // No packets received
		}
		return ReceiveBufferInfo{receiveBuffer};
	}

	/**
	 * Retrieves the data from a buffer containing a received packet.
	 *
	 * @param destination A destination buffer to read the packet's data into.
	 */
	void packet_data_get(ReceiveBufferInfo bufferInfo,
	                     uint32_t         *destination) volatile
	{
		const auto [id, size] =
		  std::pair{bufferInfo.buffer_id(), bufferInfo.size()};
		// Reception of Zero Length Packets occurs in the Status Stage of IN
		// Control Transfers.
		if (size > 0)
		{
			usbdev_transfer(destination, buffer(id), size, false);
		}
	}

	private:
	/**
	 * Return a pointer to the given offset within the USB device register
	 * space; this is used to access the packet buffer memory.
	 *
	 * @param bufferId The buffer number to access the packet buffer memory for
	 *
	 * @returns A pointer to the buffer's memory.
	 */
	uint32_t *buffer(uint8_t bufferId) volatile
	{
		const uint32_t Offset = BufferStartAddress + bufferId * MaxPacketLength;
		const uintptr_t Address = reinterpret_cast<uintptr_t>(this) + Offset;
		return const_cast<uint32_t *>(reinterpret_cast<uint32_t *>(Address));
	}

	/**
	 * Perform a transfer to or from packet buffer memory. This function is
	 * hand-optimised to perform a faster, unrolled, word-based data transfer
	 * for efficiency.
	 *
	 * @param destination A pointer to transfer the source data to.
	 * @param source A pointer to the data to be transferred.
	 * @param size The size of the data pointed to by `source`.
	 * @param toDevice True if the transfer is to the device (e.g. when sending
	 * a packet), and False if not (e.g. when receiving a packet).
	 */
	static void usbdev_transfer(uint32_t       *destination,
	                            const uint32_t *source,
	                            uint8_t         size,
	                            bool            toDevice)
	{
		// Unroll word transfer. Each word transfer is 4 bytes, so we must round
		// to the closest multiple of (4 * words) when unrolling.
		constexpr uint8_t  UnrollFactor = 4u;
		constexpr uint32_t UnrollMask   = (UnrollFactor * 4u) - 1;

		// Round down to the previous multiple for unrolling
		const uint32_t  UnrollSize = (size & ~UnrollMask);
		const uint32_t *sourceEnd  = reinterpret_cast<uint32_t *>(
          reinterpret_cast<uintptr_t>(source) + UnrollSize);

		// This is manulally unrolled for two reasons:
		// 1. We can't do partial writes to the USB packet buffer,
		//    which memcpy will attempt and causes a BUS fault.
		// 2. In the sonata system at the time of writing,
		//    the core clock is 40MHz compared to the USB device's 48MHz.
		//    This approach was found to be significantly faster than when
		//    left to compiler to optimisation.
		//
		// Ensure the unrolling here matches `UnrollFactor`.
		while (source < sourceEnd)
		{
			destination[0] = source[0];
			destination[1] = source[1];
			destination[2] = source[2];
			destination[3] = source[3];
			destination += UnrollFactor;
			source += UnrollFactor;
		}

		// Copy the remaining whole words.
		for (size &= UnrollMask; size >= UnrollFactor; size -= UnrollFactor)
		{
			*destination++ = *source++;
		}
		if (size == 0)
		{
			return;
		}

		// Copy trailing tail bytes, as USBDEV only supports 32-bit accesses.
		if (toDevice)
		{
			// Collect final bytes into a word.
			const volatile uint8_t *trailingBytes =
			  reinterpret_cast<const volatile uint8_t *>(source);
			uint32_t partialWord = trailingBytes[0];
			if (size > 1)
			{
				partialWord |= trailingBytes[1] << 8;
			}
			if (size > 2)
			{
				partialWord |= trailingBytes[2] << 16;
			}
			// Write the final word to the device.
			*destination = partialWord;
		}
		else
		{
			volatile uint8_t *destinationBytes =
			  reinterpret_cast<volatile uint8_t *>(destination);
			// Collect the final word from the device.
			const uint32_t TrailingBytes = *source;
			// Unpack it into final bytes.
			destinationBytes[0] = static_cast<uint8_t>(TrailingBytes);
			if (size > 1)
			{
				destinationBytes[1] = static_cast<uint8_t>(TrailingBytes >> 8);
			}
			if (size > 2)
			{
				destinationBytes[2] = static_cast<uint8_t>(TrailingBytes >> 16);
			}
		}
	}
};

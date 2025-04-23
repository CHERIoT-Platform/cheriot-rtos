#pragma once
#include <array>
#include <concepts>
#include <cstdint>

/**
 * Helper concept for the type returned by `receive_frame`.  This must provide a
 * buffer and a length.  It may also be an RAII type that handles any cleanup
 * when it is destroyed.
 *
 * This is not required to be copyable.
 */
template<typename T>
concept ReceivedEthernetFrame = requires(T frame) {
	{ frame->length } -> std::convertible_to<uint16_t>;
	{ frame->buffer } -> std::convertible_to<const uint8_t *>;
	{ frame->buffer } -> std::convertible_to<bool>;
};

/**
 * Concept for an Ethernet adaptor.
 */
template<typename T>
concept EthernetAdaptor = requires(T                      adaptor,
                                   const uint8_t         *buffer,
                                   uint16_t               length,
                                   std::array<uint8_t, 6> macAddress) {
	/**
	 * The default MAC address for this adaptor.  Must return a 6-byte array.
	 */
	{ T::mac_address_default() } -> std::same_as<std::array<uint8_t, 6>>;
	/**
	 * Is the default MAC address unique?  If the device (e.g. soft MAC)
	 * doesn't have its own hardware MAC address then callers may prefer to
	 * generate a locally administered MAC address randomly.
	 */
	{ T::has_unique_mac_address() } -> std::convertible_to<bool>;
	/**
	 * Set the MAC address of this adaptor.
	 */
	{ adaptor.mac_address_set(macAddress) };
	/**
	 * Set the MAC address of this adaptor to the default value.
	 */
	{ adaptor.mac_address_set() };

	/**
	 * Check if PHY link is up.
	 */
	{ adaptor.phy_link_status() } -> std::convertible_to<bool>;

	/**
	 * Receive a frame.  Returns an optional value (convertible to bool) that
	 * has a length and a buffer.  The return value owns the buffer for its
	 * lifetime.
	 */
	{ adaptor.receive_frame() } -> ReceivedEthernetFrame;

	/**
	 * Send a frame identified by a base and length.  Returns true if the frame
	 * was sent successfully, false otherwise.
	 *
	 * The third argument is a hook that allows the caller to validate the
	 * packet after it's been buffered but before it's sent.  This avoids
	 * time-of-check-to-time-of-use issues in egress filtering.
	 */
	{
		adaptor.send_frame(
		  buffer, length, [](const uint8_t * buffer, uint16_t length) {
			  return true;
		  })
	} -> std::same_as<bool>;

	/**
	 * Read the current interrupt counter for receive interrupts.  If this
	 * device doesn't support interrupts then this can return some other value.
	 * This is used only with `receive_interrupt_complete`.
	 */
	{ adaptor.receive_interrupt_value() } -> std::same_as<uint32_t>;

	/**
	 * Called after `receive_frame` fails to return new frames to block until a
	 * new frame is ready to receive.
	 *
	 * TODO: This currently blocks on a single value, this may later be
	 * augmented with an interface that sets up a multiwaiter.
	 */
	{ adaptor.receive_interrupt_complete(nullptr, 0) } -> std::same_as<int>;
};

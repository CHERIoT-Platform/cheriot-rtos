#pragma once
#include <allocator.h>
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
#include <platform/external-chips/ksz8851-spi_ethernet.hh>
#include <platform/sunburst/platform-spi.hh>
#include <thread.h>
#include <type_traits>

namespace Ksz8851
{

	DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(ethernetInterruptCapability,
	                                        InterruptName::EthernetInterrupt,
	                                        true,
	                                        true);

	/**
	 * Hardware provider for KSZ8851 SPI Ethernet MAC using the Sunburst SPI
	 * peripheral.
	 */
	template<typename Debug>
	class SunburstProvider
	{
		/**
		 * The futex used to wait for interrupts when packets are available to
		 * receive.
		 */
		const volatile uint32_t *receiveInterruptFutex;

		/// Helper. Returns a pointer to the SPI device.
		[[nodiscard]] __always_inline static volatile SonataSpi::EthernetMac *
		spi()
		{
			return MMIO_CAPABILITY(SonataSpi::EthernetMac, spi_ethmac);
		}

		public:
		SunburstProvider()
		{
			receiveInterruptFutex = interrupt_futex_get(
			  STATIC_SEALED_VALUE(ethernetInterruptCapability));
		}

		void chip_reset() const
		{
			// Hang up and reset the SPI host
			spi()->chip_select_assert(false);
			spi()->init(false, false, true, 0);

			// Reset chip. It needs to be held in reset for at least 10ms.
			spi()->reset_assert(true);
			thread_millisecond_wait(20);
			spi()->reset_assert(false);
		}

		void spi_transfer_start() const
		{
			// The SPI host is presumed idle, here.
			spi()->chip_select_assert(true);
		}

		void spi_transfer_await() const
		{
			/*
			 * All of our operations are fully synchronous, so there's nothing
			 * for which to wait.
			 */
		}

		template<bool, bool EndTransfer, bool SynchronousReturn>
		void spi_transmit(const uint8_t *outData, size_t outSize) const
		{
			// Sunburst has no DMA, so we act as if `MustCopy` is always true.

			spi()->blocking_write(outData, outSize);
			if constexpr (EndTransfer || SynchronousReturn)
			{
				spi()->wait_idle();
			}
			if constexpr (EndTransfer)
			{
				spi()->chip_select_assert(false);
			}
		}

		template<bool, bool EndTransfer, bool>
		void spi_receive(uint8_t *inData, size_t inSize) const
		{
			/*
			 * Sunburst has no DMA, so we act as if `MustCopy` is always `true`.
			 * Moreover, reception leaves the SPI host idle, and so we act as if
			 * `MustWait` is `true`.
			 */

			spi()->blocking_read(inData, inSize);
			if constexpr (EndTransfer)
			{
				spi()->chip_select_assert(false);
			}
		}

		void spi_receive_discard(uint16_t discardSize) const
		{
			spi()->blocking_discard(discardSize);
		}

		[[nodiscard]] uint32_t receive_interrupt_value() const
		{
			return *receiveInterruptFutex;
		}

		int receive_interrupt_complete(Timeout *timeout,
		                               uint32_t lastInterruptValue) const
		{
			// Our interrupt is level-triggered; if a frame happens to arrive
			// between `receive_frame` call and we marking interrupt as
			// received, it will trigger again immediately after we acknowledge
			// it.

			// Acknowledge the interrupt in the scheduler.
			interrupt_complete(
			  STATIC_SEALED_VALUE(ethernetInterruptCapability));
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
	};

} // namespace Ksz8851

using EthernetDevice = Ksz8851::Driver<Ksz8851::SunburstProvider>;
static_assert(EthernetAdaptor<EthernetDevice>);

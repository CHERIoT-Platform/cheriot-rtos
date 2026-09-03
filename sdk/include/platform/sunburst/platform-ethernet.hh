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

		[[nodiscard]] int chip_reset(TimeoutArgument) const
		{
			// Hang up and reset the SPI host
			spi()->chip_select_assert(false);
			spi()->init(false, false, true, 0);

			// Reset chip. It needs to be held in reset for at least 10ms.
			spi()->reset_assert(true);
			thread_millisecond_wait(20);
			spi()->reset_assert(false);

			return 0;
		}

		[[nodiscard]] int spi_transfer_start(TimeoutArgument) const
		{
			// The SPI host is presumed idle, here.
			spi()->chip_select_assert(true);

			return 0;
		}

		template<SPIBufferLife, SPITransferMore More>
		int spi_transmit(TimeoutArgument,
		                 const uint8_t *outData,
		                 size_t         outSize) const
		{
			/*
			 * Sunburst has no DMA, so we act as if our buffers are `MustCopy`.
			 *
			 * TODO: the Sunburst SPI platform driver does not expose timeouts
			 * on its blocking_write or wait_idle, so we ignore our timeout
			 * argument.
			 */

			spi()->blocking_write(outData, outSize);
			if constexpr (More == SPITransferMore::End)
			{
				spi()->wait_idle();
				spi()->chip_select_assert(false);
			}

			return 0;
		}

		template<SPIBufferLife, SPITransferMore More>
		int spi_receive(TimeoutArgument, uint8_t *inData, size_t inSize) const
		{
			/*
			 * Sunburst has no DMA, so we act as if our buffers are `MustCopy`.
			 *
			 * TODO: the Sunburst SPI platform driver does not expose timeouts
			 * on its blocking_read so we ignore our timeout argument.
			 */

			spi()->blocking_read(inData, inSize);
			if constexpr (More == SPITransferMore::End)
			{
				/*
				 * Unlike in spi_transmit, we don't have to call wait_idle()
				 * here, because `blocking_read` returns only after all bytes
				 * have been de-marshalled from the bus.
				 */
				spi()->chip_select_assert(false);
			}

			return 0;
		}

		[[nodiscard]] int spi_receive_discard(TimeoutArgument,
		                                      uint16_t discardSize) const
		{
			/*
			 * TODO: the Sunburst SPI platform driver does not expose timeouts
			 * on its blocking_read so we ignore our timeout argument.
			 */
			spi()->blocking_discard(discardSize);
			return 0;
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

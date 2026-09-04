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
#include <platform/sunburst/ksz8851-spi_ethernet.hh>
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

			// Reset chip. It needs to be hold in reset for at least 10ms.
			spi()->reset_assert(true);
			thread_millisecond_wait(20);
			spi()->reset_assert(false);
		}

		void spi_transfer_start() const
		{
			// The SPI host is presumed idle, here.
			spi()->chip_select_assert(true);
		}

		void spi_transfer_end() const
		{
			spi()->wait_idle();
			spi()->chip_select_assert(false);
		}

		void spi_transmit_bounce(const uint8_t *outData, size_t outSize) const
		{
			spi()->blocking_write(outData, outSize);
		}

		void spi_transmit_and_end(const uint8_t *outData, size_t outSize) const
		{
			spi_transmit_bounce(outData, outSize);
			spi_transfer_end();
		}

		void spi_receive_bounce(uint8_t *inData, size_t inSize) const
		{
			spi()->blocking_read(inData, inSize);
		}

		void spi_receive_and_end(uint8_t *inData, size_t inSize) const
		{
			spi_receive_bounce(inData, inSize);
			// reception leaves the SPI host idle; no need to wait
			spi()->chip_select_assert(false);
		}

		void spi_receive_discard(size_t inSize) const
		{
			/*
			 * This is a bit low-level, because the existing driver doesn't
			 * expose anything between this and blocking_read().
			 */
			spi()->wait_idle();
			spi()->control = SonataSpi::ControlReceiveEnable;
			for (size_t i = 0; i < inSize; ++i)
			{
				while ((spi()->status & SonataSpi::StatusRxFifoLevel) == 0)
				{
				}
				static_cast<uint8_t>(spi()->receiveFifo);
			}
		}

		uint32_t receive_interrupt_value() const
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

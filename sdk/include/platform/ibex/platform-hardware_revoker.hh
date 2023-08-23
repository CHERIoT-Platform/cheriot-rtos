// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <compartment-macros.h>
#include <futex.h>
#include <interrupt.h>
#include <riscvreg.h>
#include <stddef.h>
#include <stdint.h>

#if !DEVICE_EXISTS(revoker) && !defined(CLANG_TIDY)
#	error Memory map was not configured with a revoker device
#endif

DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY(revokerInterruptCapability,
                                        RevokerInterrupt,
                                        true,
                                        true);
namespace Ibex
{
	class HardwareRevoker
	{
		private:
		/**
		 * Layout of the revoker device.
		 */
		struct RevokerInterface
		{
			/**
			 * The base address to scan.
			 */
			uint32_t base;
			/**
			 * The top address to scan.
			 */
			uint32_t top;
			/**
			 * The control word.  The top 8 bits should be 0x55.  The next
			 * lower 8 bits are the current epoch.  The low bit is used to
			 * start the revoker.
			 */
			uint32_t control;
		};

		/**
		 * The Ibex revoker provides an 8-bit epoch.  Elsewhere, we rely on
		 * 32-bit overflow behaviour, so we maintain an internal version.
		 */
		uint32_t epoch;

		/**
		 * Get a reference to the revoker device.
		 */
		__always_inline volatile RevokerInterface &revoker_device()
		{
			return *MMIO_CAPABILITY(RevokerInterface, revoker);
		}

		/**
		 * Synchronise the epoch value with the version exported from the
		 * device.
		 *
		 * The cached epoch is updated before and after starting the revoker to
		 * ensure that the cached epoch is never more than off by one.
		 */
		void epoch_update()
		{
			// Get the current device epoch.
			auto deviceEpoch = [&]() {
				return (revoker_device().control & 0xff00) >> 8;
			};
			// If the low bits are out of sync,
			if (deviceEpoch() != (epoch & 0xff))
			{
				epoch++;
			}
			// Clang tidy is checking headers as stand-alone compilation units
			// and so doesn't know what Debug is defined to.
#ifndef CLANG_TIDY
			Debug::Assert(
			  [&]() { return deviceEpoch() == (epoch & 0xff); },
			  "Device epoch is off by more than one from cached value {}",
			  epoch);
#endif
		}

		static const uint32_t *interruptFutex;

		public:
		/**
		 * This is an asynchronous hardware revoker.
		 */
		static constexpr bool IsAsynchronous = true;

		/**
		 * Initialise a revoker instance.
		 */
		void init()
		{
			/**
			 * These two symbols mark the region that needs revocation.  We
			 * revoke capabilities everywhere from the start of compartment
			 * globals to the end of the heap.
			 */
			extern char __compart_cgps, __export_mem_heap_end;

			auto  base   = LA_ABS(__compart_cgps);
			auto  top    = LA_ABS(__export_mem_heap_end);
			auto &device = revoker_device();
			device.base  = base;
			device.top   = top;
			// Clang tidy is checking headers as stand-alone compilation units
			// and so doesn't know what Debug is defined to.
#ifndef CLANG_TIDY
			Debug::Assert((device.control >> 24) == 0x55,
			              "Device not present: {} (should be 0x55000000)",
			              device.control);
			Debug::Invariant(base < top,
			                 "Memory map has unexpected layout, base {} is "
			                 "expected to be below top {}",
			                 base,
			                 top);
#endif
			// Get a pointer to the futex that we use to wait for interrupts.
			interruptFutex = interrupt_futex_get(
			  STATIC_SEALED_VALUE(revokerInterruptCapability));
		}

		/**
		 * Returns the revocation epoch.  This is the number of revocations
		 * that have started.
		 */
		uint32_t system_epoch_get()
		{
			epoch_update();
			return epoch;
		}

		/**
		 * Queries whether the specified revocation epoch has finished.
		 */
		template<bool AllowPartial = false>
		uint32_t has_revocation_finished_for_epoch(uint32_t epoch)
		{
			auto current = system_epoch_get();
			if (AllowPartial)
			{
				return current > epoch;
			}
			return current - epoch >= (2 + (epoch & 1));
		}

		/**
		 * Start a revocation sweep.
		 */
		void system_bg_revoker_kick()
		{
			if (system_epoch_get() & 1)
			{
				return;
			}
			// If we reach here, the call to `system_epoch_get()` will have
			// updated the cached epoch to the previous-revocation-sweep-passed
			// state.
			auto &device   = revoker_device();
			device.control = 0;
			device.control = 1;
			// The epoch has started, don't bother asking the device for a new
			// value because we know it will be the last value + 1.
			epoch++;
		}

		/**
		 * Block until the revocation epoch specified by `epoch` has completed.
		 */
		bool wait_for_completion(Timeout *timeout, uint32_t epoch)
		{
			uint32_t interruptValue;
			do
			{
				// Read the current interrupt futex word.  We want to retry if
				// an interrupt happens after this point.
				interruptValue = *interruptFutex;
				// Make sure that the compiler doesn't reorder the read of the
				// futex word with respect to the read of the revocation epoch.
				__c11_atomic_signal_fence(__ATOMIC_SEQ_CST);
				// If the requested epoch has finished, return success.
				if (has_revocation_finished_for_epoch<true>(epoch))
				{
					return true;
				}
				// If the epoch hasn't finished, wait for an interrupt to fire
				// and retry.
			} while (wait_for_interrupt(timeout, interruptValue));
			// Futex wait failed.  This could be a timeout or an invalid
			// timeout parameter, we fail either way.
			return false;
		}

		private:
		/**
		 * Wait for an interrupt to complete.  Returns true if the interrupt
		 * fired, false on error or timeout.
		 */
		bool wait_for_interrupt(Timeout *timeout, uint32_t interruptValue)
		{
			if (futex_timed_wait(timeout, interruptFutex, interruptValue) == 0)
			{
				// Don't acknowledge the interrupt if another thread has
				// already.  It doesn't matter if we acknowledge an interrupt
				// twice but it's a good idea to avoid the cross-compartment
				// call if we don't need it.
				if (*interruptFutex == interruptValue + 1)
				{
					interrupt_complete(
					  STATIC_SEALED_VALUE(revokerInterruptCapability));
				}
				return true;
			}
			return false;
		}
	};
} // namespace Ibex

template<typename WordT, size_t TCMBaseAddr>
using HardwareRevoker = Ibex::HardwareRevoker;

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <compartment-macros.h>
#include <futex.h>
#include <interrupt.h>
#include <limits>
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
			 * The control word.  The top 16 bits should be 0x5500.  Writing to
			 * the low bit will start the revoker.
			 */
			uint32_t control;
			/**
			 * The revocation epoch.  Low bit indicates that the revoker is
			 * running.
			 */
			uint32_t epoch;
			/**
			 * Interrupt status word.  Reading this will return 0 if an
			 * interrupt has not been requested, 1 otherwise.  Writing 1 clears
			 * any pending interrupt.
			 */
			uint32_t interruptStatus;
			/**
			 * Interrupt request word.  Writing 1 here requests an interrupt to
			 * fire when the current revocation has completed.
			 */
			uint32_t interruptRequested;
		};

		/**
		 * Get a reference to the revoker device.
		 */
		__always_inline volatile RevokerInterface &revoker_device()
		{
			return *MMIO_CAPABILITY(RevokerInterface, revoker);
		}

		static inline const uint32_t *interruptFutex;

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
			extern char __revoker_scan_start, __export_mem_heap_end;

			auto  base   = LA_ABS(__revoker_scan_start);
			auto  top    = LA_ABS(__export_mem_heap_end);
			auto &device = revoker_device();
			device.base  = base;
			device.top   = top;
			// Clang tidy is checking headers as stand-alone compilation units
			// and so doesn't know what Debug is defined to.
#ifndef CLANG_TIDY
			Debug::Assert((device.control >> 16) == 0x5500,
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
			return revoker_device().epoch;
		}

		/**
		 * Queries whether the specified revocation epoch has finished, or,
		 * if `AllowPartial` is true, that it has (at least) started.
		 *
		 * `epoch` must be even, as memory leaves quarantine only when
		 * revocation is not in progress.
		 */
		template<bool AllowPartial = false>
		uint32_t has_revocation_finished_for_epoch(uint32_t epoch)
		{
			auto current = system_epoch_get();
			// We want to know if current is greater than epoch, but current
			// may have wrapped.  Perform unsigned subtraction (guaranteed to
			// wrap) and then coerce the result to a signed value.  This will
			// be correct unless we have more than 2^31 revocations in between
			// checks
			std::make_signed_t<decltype(current)> distance = current - epoch;
			if (AllowPartial)
			{
				return distance >= 0;
			}
			// The allocator stores the epoch when things can be popped, which
			// is always a complete (even) epoch.
#ifndef CLANG_TIDY
			Debug::Assert((epoch & 1) == 0, "Epoch must be even");
#endif
			// If the current epoch is odd then the epoch needs to be at least
			// two more, to capture the fact that this is a complete epoch.
			decltype(distance) minimumRequired = 1 + (epoch & 1);
			return distance > minimumRequired;
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
			auto &device   = revoker_device();
			device.control = 0;
			device.control = 1;
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
				if (has_revocation_finished_for_epoch(epoch))
				{
					return true;
				}
				// Request the interrupt
				revoker_device().interruptRequested = 1;
				// There is a possible race: if the revocation pass finished
				// before we requested the interrupt, we won't get the
				// interrupt.  Check again before we wait.
				if (has_revocation_finished_for_epoch(epoch))
				{
					return true;
				}
				// Make sure that the revoker is running.
				system_bg_revoker_kick();
				// If the epoch hasn't finished, wait for an interrupt to fire
				// and retry.
			} while (
			  futex_timed_wait(timeout, interruptFutex, interruptValue) == 0);
			// Futex wait failed.  This could be a timeout or an invalid
			// timeout parameter, we fail either way.
			return false;
		}
	};
} // namespace Ibex

template<typename WordT, size_t TCMBaseAddr>
using HardwareRevoker = Ibex::HardwareRevoker;

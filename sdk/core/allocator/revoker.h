// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include "alloc_config.h"
#include "software_revoker.h"
#include <concepts>
#include <riscvreg.h>
#include <stdint.h>
#include <utils.hh>

#include <platform/concepts/hardware_revoker.hh>
#include <revoker/bitmap_direct.hh>

#if defined(TEMPORAL_SAFETY) && !defined(SOFTWARE_REVOKER)
#	if __has_include(<platform-hardware_revoker.hh>)
#		include <platform-hardware_revoker.hh>
#	else
#		error Hardware revoker requested but no hardware_revoker.hh found
#	endif
#endif

namespace Revocation
{
	/**
	 * Null implementation of the revoker interface.  Provides no temporal
	 * safety.
	 */
	class NoTemporalSafety
	{
		public:
		/**
		 * If there is no temporal memory safety, we treat sweeping as
		 * synchronous but infinitely fast which returns immediately.
		 */
		static constexpr bool IsAsynchronous = false;

		void init() {}

		uint32_t system_epoch_get()
		{
			return 0;
		}

		template<bool AllowPartial = true>
		uint32_t has_revocation_finished_for_epoch(uint32_t previousEpoch)
		{
			return true;
		}

		void system_bg_revoker_kick() {}

		bool mark_get(ptraddr_t addr)
		{
			Debug::Assert(false,
			              "shadow_bit_get should not be called on the revoker "
			              "with no temporal safety, its result is meaningless");
			return false;
		}

		void mark_set_one(CHERI::Capability<void>) {}

		void mark_clear_one(ptraddr_t) {}

		void mark_set_range(CHERI::Capability<void>) {}

		void mark_clear_range(size_t, size_t) {}
	};

	/**
	 * Software revoker for use with a hardware load barrier.  Uses a separate
	 * (very privileged!) compartment to scan all of memory.
	 */
	template<typename WordT, ptraddr_t TCMBaseAddr>
	class SoftwareRevoker : public BitmapDirect<WordT, TCMBaseAddr>
	{
		private:
		/**
		 * A (read-only) pointer to the revocation epoch.  Incremented once
		 * when revocation starts and once when it finishes.
		 */
		const uint32_t *epoch;

		public:
		/**
		 * Software sweeping is implemented synchronously now. The sweeping is
		 * done when memory is under pressure or malloc() failed. malloc() and
		 * free() only return when a certain amount of sweeping is done.
		 */
		static constexpr bool IsAsynchronous = false;

		/**
		 * Initialise the software revoker.
		 */
		void init()
		{
			BitmapDirect<WordT, TCMBaseAddr>::init();
			epoch = revoker_epoch_get();
		}

		/**
		 * Returns the revocation epoch.  This is the number of revocations
		 * that have started or finished.  It will be even if revocation is not
		 * running.
		 */
		uint32_t system_epoch_get()
		{
			return *epoch;
		}

		/**
		 * Queries whether the specified revocation epoch has finished.
		 */
		template<bool AllowPartial = false>
		uint32_t has_revocation_finished_for_epoch(uint32_t previousEpoch)
		{
			auto current = *epoch;
			// If the revoker is running, prod it to do a bit more work every
			// time that it's queried.
			if ((current & 1) == 1)
			{
				(void)revoker_tick();
				current = *epoch;
			}
			// We want to know if current is greater than epoch, but current
			// may have wrapped.  Perform unsigned subtraction (guaranteed to
			// wrap) and then coerce the result to a signed value.  This will
			// be correct unless we have more than 2^31 revocations in between
			// checks
			std::make_signed_t<decltype(current)> distance =
			  current - previousEpoch;
			if (AllowPartial)
			{
				return distance >= 0;
			}
			// The allocator stores the epoch when things can be popped, which
			// is always a complete (even) epoch.
			Debug::Assert((previousEpoch & 1) == 0, "Epoch must be even");
			// If the current epoch is odd then the epoch needs to be at least
			// two more, to capture the fact that this is a complete epoch.
			decltype(distance) minimumRequired = 1 + (previousEpoch & 1);
			return distance > minimumRequired;
		}

		/// Start revocation running.
		void system_bg_revoker_kick()
		{
			(void)revoker_tick();
		}
	};

	template<typename WordT, ptraddr_t TCMBaseAddr>
	class FakeRevoker : public BitmapDirect<WordT, TCMBaseAddr>
	{
		private:
		/**
		 * A (read-only) pointer to the revocation epoch.  Incremented once
		 * when revocation starts and once when it finishes.
		 */
		uint32_t epoch;

		public:
		/**
		 * Software sweeping is implemented synchronously now. The sweeping is
		 * done when memory is under pressure or malloc() failed. malloc() and
		 * free() only return when a certain amount of sweeping is done.
		 */
		static constexpr bool IsAsynchronous = false;

		/**
		 * Initialise the software revoker.
		 */
		void init()
		{
			BitmapDirect<WordT, TCMBaseAddr>::init();
			epoch = 0;
		}

		/**
		 * Returns the revocation epoch.  This is the number of revocations
		 * that have started or finished.  It will be even if revocation is not
		 * running.
		 */
		uint32_t system_epoch_get()
		{
			return epoch;
		}

		/**
		 * Queries whether the specified revocation epoch has finished.
		 */
		template<bool AllowPartial = false>
		uint32_t has_revocation_finished_for_epoch(uint32_t previousEpoch)
		{
			return true;
		}

		/// Start revocation running.  Fake revocation completes instantly.
		void system_bg_revoker_kick()
		{
			epoch++;
		}
	};
	/**
	 * The revoker to use for this configuration.
	 *
	 * FIXME: This should not hard-code the start address.
	 */
	using Revoker =
#ifdef TEMPORAL_SAFETY
#	ifdef SOFTWARE_REVOKER
	  SoftwareRevoker<uint32_t, REVOKABLE_MEMORY_START>
#	else
	  HardwareRevoker<uint32_t, REVOKABLE_MEMORY_START>
#	endif
#else
#	ifdef CHERIOT_FAKE_REVOKER
	  FakeRevoker<uint32_t, REVOKABLE_MEMORY_START>;
#	else
	  NoTemporalSafety
#	endif
#endif
	  ;

	static_assert(IsRevokerDevice<Revoker>);
} // namespace Revocation

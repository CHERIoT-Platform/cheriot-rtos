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

#if __has_include(<platform-hardware_revoker.hh>)
#	include <platform-hardware_revoker.hh>
#elif defined(TEMPORAL_SAFETY) && !defined(SOFTWARE_REVOKER)
#	error Hardware revoker requested but no hardware_revoker.hh found
#endif

namespace Revocation
{
	/**
	 * Class for interacting with the shadow bitmap.  This bitmap controls the
	 * behaviour of a hardware load barrier, which will invalidate capabilities
	 * in the register file if they point revoked memory.  Only memory that is
	 * intended for use as a heap is required to use the load barrier and so
	 * the revocation bitmap is not required to span the whole of the address
	 * space.
	 *
	 * Template arguments are the size of the value to be used for reads and
	 * updates, which may vary depending on the width of the interface to the
	 * shadow memory, and the base address of the memory covered by the shadow
	 * bitmap.
	 *
	 * This currently assumes that all revocable memory is in a single
	 * contiguous region.
	 */
	template<typename WordT, size_t TCMBaseAddr>
	class Bitmap
	{
		/// The pointer to the shadow bitmap region.
		WordT *shadowCap;

		/// The number of bits in a single load or store of the shadow memory.
		static constexpr size_t ShadowWordSizeBits =
		  utils::bytes2bits(sizeof(*shadowCap));

		/// The shift required to translate an offset in bytes in memory into
		/// an offset in bytes in the shadow memory.
		static constexpr size_t ShadowWordShift =
		  utils::log2<ShadowWordSizeBits>();

		/// The mask used to compute which bits to update in a word of the
		/// revocation bitmap.
		static constexpr size_t ShadowWordMask = (1U << ShadowWordShift) - 1;

		protected:
		/// Initialise this class with a capability to the shadow bitmap.
		void init()
		{
			shadowCap = const_cast<WordT *>(MMIO_CAPABILITY(WordT, shadow));
		}

		static constexpr size_t shadow_offset_bits(ptraddr_t addr)
		{
			return (addr - TCMBaseAddr) >> MallocAlignShift;
		}

		static constexpr size_t shadow_word_index(size_t offsetBits)
		{
			return offsetBits >> ShadowWordShift;
		}

		static constexpr WordT shadow_mask_bits_below_address(ptraddr_t addr)
		{
			size_t capoffset = shadow_offset_bits(addr);
			return (WordT(1) << (capoffset & ShadowWordMask)) - 1;
		}

		static constexpr WordT shadow_mask_bits_above_address(ptraddr_t addr)
		{
			return ~shadow_mask_bits_below_address(addr);
		}

		/*
		 * Some quick verification that the union of these two masks is
		 * all bits set.
		 */
		static_assert((shadow_mask_bits_above_address(0) ^
		               shadow_mask_bits_below_address(0)) == WordT(-1));
		static_assert((shadow_mask_bits_above_address(7) ^
		               shadow_mask_bits_below_address(7)) == WordT(-1));
		static_assert((shadow_mask_bits_above_address(ShadowWordSizeBits - 1) ^
		               shadow_mask_bits_below_address(ShadowWordSizeBits -
		                                              1)) == WordT(-1));

		public:
		/**
		 * @brief Set or clear the single shadow bit for an address.
		 *
		 * @param fill true to set, false to clear the bit
		 */
		void shadow_paint_single(ptraddr_t addr, bool fill)
		{
			Debug::Assert(addr > TCMBaseAddr,
			              "Address {} is below the TCM base {}",
			              addr,
			              TCMBaseAddr);
			size_t capoffset  = shadow_offset_bits(addr);
			WordT  shadowWord = shadowCap[shadow_word_index(capoffset)];
			WordT  mask       = (WordT(1) << (capoffset & ShadowWordMask));

			if (fill)
			{
				shadowWord |= mask;
			}
			else
			{
				shadowWord &= ~mask;
			}
			shadowCap[shadow_word_index(capoffset)] = shadowWord;
		}

		/**
		 * @brief Set or clear the shadow bits for all addresses within [base,
		 * top).
		 *
		 * @param Fill true to set, false to clear the bits
		 */
		template<bool Fill>
		__always_inline void shadow_paint_range(ptraddr_t base, ptraddr_t top)
		{
			size_t baseCapOffset = shadow_offset_bits(base);
			size_t topCapOffset  = shadow_offset_bits(top);
			size_t baseWordIx    = shadow_word_index(baseCapOffset);
			size_t topWordIx     = shadow_word_index(topCapOffset);

			WordT maskHi = shadow_mask_bits_below_address(top);
			WordT maskLo = shadow_mask_bits_above_address(base);

			if (baseWordIx == topWordIx)
			{
				/*
				 * This object is entirely contained within one word of the
				 * bitmap.  We must AND the mask{Hi,Lo} together, since those
				 * masks were assuming that the object ran to (or past) the
				 * respective ends of the word.
				 */
				WordT mask = maskHi & maskLo;
				if (Fill)
				{
					shadowCap[baseWordIx] |= mask;
				}
				else
				{
					shadowCap[baseWordIx] &= ~mask;
				}

				return;
			}

			/*
			 * Otherwise, there are at least two words of the bitmap that need
			 * to be updated with the masks, and possibly some in between that
			 * just need to be set wholesale.
			 *
			 * We paint ranges "backwards", from highest address to lowest, so
			 * that we never create a window in which an interior pointer has a
			 * clear shadow bit while the lower adjacent address has an asserted
			 * shadow bit, as that would open the door to confusing the interior
			 * pointer with a pointer to the start of an object (recall that
			 * object headers are marked in the shadow bitmap).
			 *
			 * When clearing ranges, the order matters less.  A correct
			 * allocator will have run revocation first, and so there should be
			 * no interior pointers (outside the allocator, anyway) to worry us.
			 */
			WordT midWord;
			if constexpr (Fill)
			{
				shadowCap[topWordIx] |= maskHi;
				midWord = ~WordT(0);
			}
			else
			{
				shadowCap[topWordIx] &= ~maskHi;
				midWord = 0;
			}

			/*
			 * This loop is underflow-safe, since topWordIx is strictly greater
			 * than baseWordIx after the test for equality above.
			 */
			for (size_t shadowWordIx = topWordIx - 1; baseWordIx < shadowWordIx;
			     shadowWordIx--)
			{
				shadowCap[shadowWordIx] = midWord;
			}

			if constexpr (Fill)
			{
				shadowCap[baseWordIx] |= maskLo;
			}
			else
			{
				shadowCap[baseWordIx] &= ~maskLo;
			}
		}

		// Return the shadow bit at address addr.
		bool shadow_bit_get(size_t addr)
		{
			size_t capoffset  = shadow_offset_bits(addr);
			WordT  shadowWord = shadowCap[shadow_word_index(capoffset)];
			WordT  mask       = (WordT(1) << (capoffset & ShadowWordMask));

			return (shadowWord & mask) != 0;
		}

		/**
		 * @brief Check whether the argument of free() is valid.
		 * Because we use shadow bits to mark allocation boundaries, this has to
		 * be a method of a revoker instance.
		 *
		 * @return true if the input is valid
		 */
		bool is_free_cap_valid(CHERI::Capability<void> cap)
		{
			ptraddr_t base = cap.base();

			return cap.is_valid() && (base & MallocAlignMask) == 0 &&
			       shadow_bit_get(base - MallocAlignment) == 1 &&
			       shadow_bit_get(base) == 0;
		}
	};

	template<typename WordT,
	         size_t TCMBaseAddr,
	         template<typename, size_t>
	         typename Revoker>
	    requires IsHardwareRevokerDevice<Revoker<WordT, TCMBaseAddr>>
	class HardwareAccelerator : public Bitmap<WordT, TCMBaseAddr>,
	                            public Revoker<WordT, TCMBaseAddr>
	{
		public:
		/**
		 * Currently the only hardware revoker implementation is async which
		 * sweeps memory in the background.
		 */
		static constexpr bool IsAsynchronous = true;

		/**
		 * Initialise a revoker instance.
		 */
		void init()
		{
			Bitmap<WordT, TCMBaseAddr>::init();
			Revoker<WordT, TCMBaseAddr>::init();
		}
	};

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
		void                  init() {}
		void                  shadow_paint_single(size_t, bool) {}
		template<bool Fill>
		void shadow_paint_range(size_t, size_t)
		{
		}
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
		bool is_free_cap_valid(void *)
		{
			return true;
		}
		bool shadow_bit_get(size_t addr)
		{
			Debug::Assert(false,
			              "shadow_bit_get should not be called on the revoker "
			              "with no temporal safety, its result is meaningless");
			return false;
		}
	};

	/**
	 * Software revoker for use with a hardware load barrier.  Uses a separate
	 * (very privileged!) compartment to scan all of memory.
	 */
	template<typename WordT, size_t TCMBaseAddr>
	class SoftwareRevoker : public Bitmap<WordT, TCMBaseAddr>
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
			Bitmap<WordT, TCMBaseAddr>::init();
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

	template<typename WordT, size_t TCMBaseAddr>
	class FakeRevoker : public Bitmap<WordT, TCMBaseAddr>
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
			Bitmap<WordT, TCMBaseAddr>::init();
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
	  HardwareAccelerator<uint32_t, REVOKABLE_MEMORY_START, HardwareRevoker>
#	endif
#else
#	ifdef CHERIOT_FAKE_REVOKER
	  FakeRevoker<uint32_t, REVOKABLE_MEMORY_START>;
#	else
	  NoTemporalSafety
#	endif
#endif
	  ;
} // namespace Revocation

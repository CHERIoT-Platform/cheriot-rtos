// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cheri.hh>
#include <concepts>
#include <debug.hh>
#include <riscvreg.h>
#include <stdint.h>
#include <utils.hh>

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
	template<typename WordT, ptraddr_t TCMBaseAddr>
	class BitmapDirect
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
			return (addr - TCMBaseAddr) >> utils::log2<sizeof(void *)>();
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

		/**
		 * @brief Set or clear the single shadow bit for an address.
		 *
		 * @param fill true to set, false to clear the bit
		 */
		void mark_one(ptraddr_t address, bool fill)
		{
#if !defined(CLANG_TIDY)
			Debug::Assert(address > TCMBaseAddr,
			              "Address {} is below the TCM base {}",
			              address,
			              TCMBaseAddr);
#endif
			size_t capoffset  = shadow_offset_bits(address);
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
		__always_inline void mark_range(ptraddr_t base, ptraddr_t top)
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

		public:
		/**
		 * @brief Set a single shadow bit for an address.
		 */
		__always_inline void mark_set_one(CHERI::Capability<void> cap)
		{
			mark_one(cap.address(), true);
		}

		/**
		 * @brief Clear a single shadow bit for an address.
		 */
		__always_inline void mark_clear_one(ptraddr_t address)
		{
			mark_one(address, false);
		}

		/**
		 * @brief Set a range of shadow bits, from cap's base to its top.
		 */
		__always_inline void mark_set_range(CHERI::Capability<void> cap)
		{
			mark_range<true>(cap.base(), cap.top());
		}

		/**
		 * @brief Clear a range of shadow bits, from base to top.
		 *
		 * This takes a pair of addresses rather than a capability since a
		 * precisely-bounded capability to the region in question would not
		 * be round-trippable through memory, even if the allocator could
		 * construct such a thing (since it retains elevated access to the heap
		 * region).
		 */
		__always_inline void mark_clear_range(ptraddr_t base, ptraddr_t top)
		{
			mark_range<false>(base, top);
		}

		// Return the shadow bit at address addr.
		bool mark_get(size_t addr)
		{
			size_t capoffset  = shadow_offset_bits(addr);
			WordT  shadowWord = shadowCap[shadow_word_index(capoffset)];
			WordT  mask       = (WordT(1) << (capoffset & ShadowWordMask));

			return (shadowWord & mask) != 0;
		}
	};
} // namespace Revocation

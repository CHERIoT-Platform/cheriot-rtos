// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include "alloc_config.h"
#include "software_revoker.h"
#include <riscvreg.h>
#include <stdint.h>
#include <utils.h>

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

		public:
		/**
		 * @brief Set or clear the single shadow bit for an address.
		 *
		 * @param fill true to set, false to clear the bit
		 */
		void shadow_paint_single(size_t addr, bool fill)
		{
			Debug::Assert(addr > TCMBaseAddr,
			              "Address {} is below the TCM base {}",
			              addr,
			              TCMBaseAddr);
			size_t   capoffset  = (addr - TCMBaseAddr) >> MALLOC_ALIGNSHIFT;
			WordT    shadowWord = shadowCap[capoffset >> ShadowWordShift];
			uint32_t mask       = (1U << (capoffset & ShadowWordMask));

			if (fill)
			{
				shadowWord |= mask;
			}
			else
			{
				shadowWord &= ~mask;
			}
			shadowCap[capoffset >> ShadowWordShift] = shadowWord;
		}

		/**
		 * @brief Set or clear the shadow bits for all addresses within [base,
		 * top).
		 *
		 * @param fill true to set, false to clear the bits
		 */
		void shadow_paint_range(size_t base, size_t top, bool fill)
		{
			constexpr size_t ShadowWordAddrMask =
			  (1U << (ShadowWordShift + MALLOC_ALIGNSHIFT)) - 1;
			size_t baseUp  = (base + ShadowWordAddrMask) & ~ShadowWordAddrMask;
			size_t topDown = top & ~ShadowWordAddrMask;

			// There isn't a single aligned shadow word for this range, so paint
			// one bit at a time.
			if (baseUp >= topDown)
			{
				for (size_t ptr = base; ptr < top; ptr += MALLOC_ALIGNMENT)
				{
					shadow_paint_single(ptr, fill);
				}
				return;
			}

			// First, paint the individual bits at the beginning.
			for (size_t ptr = base; ptr < baseUp; ptr += MALLOC_ALIGNMENT)
			{
				shadow_paint_single(ptr, fill);
			}
			// Then, paint the aligned shadow words using word instructions.
			for (size_t ptr = baseUp; ptr < topDown;
			     ptr += ShadowWordSizeBits * MALLOC_ALIGNMENT)
			{
				size_t capoffset  = (ptr - TCMBaseAddr) >> MALLOC_ALIGNSHIFT;
				WordT  shadowWord = fill ? -1 : 0;

				shadowCap[capoffset >> ShadowWordShift] = shadowWord;
			}
			// Finally, paint individual bits at the end.
			for (size_t ptr = topDown; ptr < top; ptr += MALLOC_ALIGNMENT)
			{
				shadow_paint_single(ptr, fill);
			}
		}

		// Return the shadow bit at address addr.
		bool shadow_bit_get(size_t addr)
		{
			size_t   capoffset  = (addr - TCMBaseAddr) >> MALLOC_ALIGNSHIFT;
			WordT    shadowWord = shadowCap[capoffset >> ShadowWordShift];
			uint32_t mask       = (1U << (capoffset & ShadowWordMask));

			return shadowWord & mask;
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

			return cap.is_valid() && (base & MALLOC_ALIGNMASK) == 0 &&
			       shadow_bit_get(base - MALLOC_ALIGNMENT) == 1 &&
			       shadow_bit_get(base) == 0;
		}
	};

	template<typename WordT, size_t TCMBaseAddr>
	class HardwareAccelerator : public Bitmap<WordT, TCMBaseAddr>
	{
		private:
		// layout of the shadow space control registers
		struct ShadowCtrl
		{
			uint32_t base;
			uint32_t pad0;
			uint32_t top;
			uint32_t pad1;
			uint32_t enqEpoch;
			uint32_t pad2;
			uint32_t deqEpoch;
			uint32_t pad3;
			uint32_t go;
			uint32_t pad4;
		};

		volatile ShadowCtrl *shadowCtrl;

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
			/**
			 * These two symbols mark the region that needs revocation.  We
			 * revoke capabilities everywhere from the start of compartment
			 * globals to the end of the heap.
			 */
			extern char __compart_cgps, __export_mem_heap_end;

			auto base = LA_ABS(__compart_cgps);
			auto top  = LA_ABS(__export_mem_heap_end);
			Bitmap<WordT, TCMBaseAddr>::init();
			shadowCtrl       = MMIO_CAPABILITY(ShadowCtrl, shadowctrl);
			shadowCtrl->base = base;
			shadowCtrl->top  = top;
			Debug::Invariant(base < top,
			                 "Memory map has unexpected layout, base {} is "
			                 "expected to be below top {}",
			                 base,
			                 top);
		}

		/**
		 * Returns the revocation epoch.  This is the number of revocations
		 * that have started.
		 */
		uint32_t system_epoch_get()
		{
			return shadowCtrl->enqEpoch;
		}

		/**
		 * Queries whether the specified revocation epoch has finished.
		 */
		uint32_t has_revocation_finished_for_epoch(uint32_t epoch)
		{
			/*
			 * Currently we use a 2-counter design. The enq counter starts at
			 * deq + 1. Enq counter += 1 when the revocation starts and deq +=
			 * 1 when a background revocation is done. We know a chunk has gone
			 * through a full revocation start and finish when the system deq
			 * counter has caught up with the enq stamp of this chunk.
			 */
			return shadowCtrl->deqEpoch >= epoch;
		}

		// Start a revocation.
		void system_bg_revoker_kick()
		{
			shadowCtrl->go = 1;
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
		void                  shadow_paint_range(size_t, size_t, bool) {}
		uint32_t              system_epoch_get()
		{
			return 0;
		}
		uint32_t has_revocation_finished_for_epoch(uint32_t previousEpoch)
		{
			return true;
		}
		void system_bg_revoker_kick() {}
		bool is_free_cap_valid(void *)
		{
			return true;
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
		uint32_t has_revocation_finished_for_epoch(uint32_t previousEpoch)
		{
			// If the revoker is running, prod it to do a bit more work every
			// time that it's queried.
			if ((*epoch & 1) == 1)
			{
				revoker_tick();
			}
			// We want to check if a complete revocation pass has happened
			// since the last query of the epoch.  If the last query happened
			// in the middle of revocation then the low bit will be 1.  In this
			// case, we need to ensure that the revocation epoch has
			// incremented by 3 (finished the in-progress sweep, started and
			// finished the next one).  In the other case, where revocation was
			// not in progress, we need to ensure only that one complete
			// revocation pass has happened and so the epoch counter was
			// incremented twice.
			//
			// We perform a subtraction first because unsigned overflow is
			// well-defined in C to wrap and so, as long as we don't manage to
			// do 2^31 revocation sweeps without the consumer noticing then
			// this will give the correct result even in overflow cases.
			return *epoch - previousEpoch >= (2 + (previousEpoch & 1));
		}

		/// Start revocation running.
		void system_bg_revoker_kick()
		{
			revoker_tick();
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
	  SoftwareRevoker<uint32_t, 0x80000000>
#	else
	  HardwareAccelerator<uint32_t, 0x80000000>
#	endif
#else
	  NoTemporalSafety
#endif
	  ;
} // namespace Revocation

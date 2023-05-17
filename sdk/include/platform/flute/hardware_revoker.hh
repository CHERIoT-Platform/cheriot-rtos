// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <compartment-macros.h>
#include <riscvreg.h>
#include <stddef.h>
#include <stdint.h>

namespace Flute
{
	template<typename WordT, size_t TCMBaseAddr>
	class HardwareRevoker
	{
		private:
		// layout of the shadow space control registers
		struct ShadowCtrl
		{
			uint32_t base;
			uint32_t pad0;
			uint32_t top;
			uint32_t pad1;
			uint32_t epoch;
			uint32_t pad2;
			uint32_t go;
			uint32_t pad4;
		};
		static_assert(offsetof(ShadowCtrl, epoch) == 16);
		static_assert(offsetof(ShadowCtrl, go) == 24);

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

			auto base        = LA_ABS(__compart_cgps);
			auto top         = LA_ABS(__export_mem_heap_end);
			shadowCtrl       = MMIO_CAPABILITY(ShadowCtrl, shadowctrl);
			shadowCtrl->base = base;
			shadowCtrl->top  = top;
			// Clang tidy is checking headers as stand-alone compilation units
			// and so doesn't know what Debug is defined to.
#ifndef CLANG_TIDY
			Debug::Invariant(base < top,
			                 "Memory map has unexpected layout, base {} is "
			                 "expected to be below top {}",
			                 base,
			                 top);
#endif
		}

		/**
		 * Returns the revocation epoch.  This is the number of revocations
		 * that have started.
		 */
		uint32_t system_epoch_get()
		{
			asm volatile("" ::: "memory");
			return shadowCtrl->epoch;
		}

		/**
		 * Queries whether the specified revocation epoch has finished.
		 */
		template<bool AllowPartial = false>
		uint32_t has_revocation_finished_for_epoch(uint32_t epoch)
		{
			asm volatile("" ::: "memory");
			if (AllowPartial)
			{
				return shadowCtrl->epoch > epoch;
			}
			return shadowCtrl->epoch - epoch >= (2 + (epoch & 1));
		}

		// Start a revocation.
		void system_bg_revoker_kick()
		{
			asm volatile("" ::: "memory");
			shadowCtrl->go = 1;
			asm volatile("" ::: "memory");
		}
	};
} // namespace Flute

template<typename WordT, size_t TCMBaseAddr>
using HardwareRevoker = Flute::HardwareRevoker<WordT, TCMBaseAddr>;

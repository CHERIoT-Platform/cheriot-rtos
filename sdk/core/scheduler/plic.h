// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "common.h"
#include "event.h"
#include <compartment.h>
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils.h>

/*
 * Platform specific low-level implementations of the interrupt controller. Not
 * to be used directly. These should be inherited by the interrupt controller
 * wrapper class.
 */
namespace
{
	using InterruptHandler = bool (*)(void *);
	using Priority         = uint32_t;
	using SourceID         = uint32_t;

	/**
	 * @brief this is the standardised RISC-V PLIC
	 * @param MaxIntrID the max ID (inclusive) of supported interrupts.
	 */
	template<size_t MaxIntrID>
	class StandardPlic : private utils::NoCopyNoMove
	{
		public:
		StandardPlic()
		{
			volatile uint32_t *range    = MMIO_CAPABILITY(uint32_t, plic);
			size_t             nSources = (MaxIntrID + 32U) & ~0x1f;
			// We program the enable bits in groups of 32.
			size_t nSourcesGroups = nSources >> 5;

			auto setField = [&](auto &field, size_t offset, size_t size) {
				CHERI::Capability capability{range};
				capability.address() += offset;
				capability.bounds() = size;
				field               = capability;
			};
			setField(plicPrios, PriorityOffset, nSources * sizeof(uint32_t));
			setField(plicPendings, PendingOffset, nSources / 8);
			setField(plicEnables, EnableOffset, nSources / 8);
			setField(plicThres, ThresholdOffset, sizeof(uint32_t));
			setField(plicClaim, ClaimOffset, sizeof(uint32_t));

			for (size_t i = 0; i < nSourcesGroups; i++)
			{
				plicEnables[i] = 0U;
			}
			for (size_t i = 1; i <= MaxIntrID; i++)
			{
				plicPrios[i] = 0U;
			}
			// We don't make use of threshold control. Leave it at 0 so all
			// configured interrupts can fire.
			*plicThres = 0U;
		}

		void intr_enable(SourceID src)
		{
			size_t idx    = src >> 5;
			size_t bitPos = src & 0x1f;

			uint32_t enables = plicEnables[idx] | (1U << bitPos);
			plicEnables[idx] = enables;
		}
		void intr_disable(SourceID src)
		{
			size_t idx    = src >> 5;
			size_t bitPos = src & 0x1f;

			uint32_t enables = plicEnables[idx] & ~(1U << bitPos);
			plicEnables[idx] = enables;
		}

		void priority_set(SourceID src, Priority prio)
		{
			plicPrios[src] = prio;
		}

		/**
		 * Fetch the interrupt number that fired and prevent it from firing.
		 * @return real value when there is a real interrupt pending. nullopt
		 *         when there is nothing
		 */
		std::optional<SourceID> intr_claim()
		{
			uint32_t claim = *plicClaim;
			// PLIC reserves source ID 0, which means no interrupts.
			return claim == 0 ? std::nullopt : std::optional{claim};
		}
		/**
		 * Tell the interrupt controller we've handled interrupt ID src.
		 * src can now fire again.
		 */
		void intr_complete(SourceID src)
		{
			*plicClaim = src;
		}

		~StandardPlic() = default;

		private:
		// generic offsets according to spec
		static constexpr size_t PriorityOffset  = 0x0U;
		static constexpr size_t PendingOffset   = 0x1000U;
		static constexpr size_t EnableOffset    = 0x2000U;
		static constexpr size_t ThresholdOffset = 0x200000U;
		static constexpr size_t ClaimOffset     = 0x200004U;

		// Ideally these should be contained in one volatile struct, but
		// they are so far apart.
		volatile uint32_t *plicPrios;
		volatile uint32_t *plicPendings;
		volatile uint32_t *plicEnables;
		volatile uint32_t *plicThres;
		volatile uint32_t *plicClaim;
	};
} // namespace

namespace sched
{
/*
 * FIXME: Sail doesn't have an interrupt controller at all, but we pretend it
 * does just like FLUTE build to let things compile. We need tons of #ifdefs or
 * a big rewrite to make the entire external interrupt path optional.
 *
 * FIXME: Here should only be platform-agnostic code but we still hardcode an
 * Ethernet handler. We should be generic and auto-generate event channels and
 * intr_complete() functions here.
 */
#if defined(FLUTE) || defined(SAIL)
	static constexpr size_t   InterruptsNum    = 1;
	static constexpr SourceID EthernetSourceId = 2;
	static constexpr Priority EthernetPriority = 6;
	using PlicCore                             = StandardPlic<InterruptsNum>;
#else
#	error "Unsupported interrupt controller"
#endif

	/**
	 * The PLIC class that wraps platform-specific implementations and
	 * provides higher-level abstractions.
	 */
	class Plic final : public PlicCore
	{
		public:
		static Plic &master()
		{
			// masterPlic is the statically-allocated space for the main Plic,
			// and we know it's been placement-newed during boot in
			// master_init(), so it contains a valid Plic object.
			return reinterpret_cast<Plic &>(masterPlic);
		}

		static void master_init()
		{
			// The Clang analyser has a false positive here, not seeing the
			// `alignas` directive on masterPlic
			new (masterPlic) // NOLINT(clang-analyzer-cplusplus.PlacementNew)
			  Plic();
		}

		Plic() : PlicCore()
		{
			// Populate the interrupt event channel for devices.
			new (ethernetEvent) Event();
			for (auto &handler : handlerTable)
			{
				handler.handler = nullptr;
			}
			// Register interrupt handlers.
			intr_handler_register(
			  EthernetSourceId, ethernet_intr_handler, ethernetEvent);
			// Set and enable device interrupts.
			priority_set(EthernetSourceId, EthernetPriority);
			intr_enable(EthernetSourceId);
		}

		Event *ethernet_event_get()
		{
			return compart_seal(reinterpret_cast<Event *>(ethernetEvent));
		}

		void ethernet_intr_complete()
		{
			intr_complete(EthernetSourceId);
		}

		/**
		 * Handling external interrupts.
		 * @return true if a task is preempted and needs reschedule
		 */
		[[nodiscard]] bool do_external_interrupt()
		{
			std::optional<SourceID> src = intr_claim();
			if (!src)
			{
				// We entered external interrupt but for whatever reason the
				// interrupt controller says it sees nothing, so just return.
				return false;
			}

			auto entry = src2handler(*src);
			Debug::Invariant(
			  entry != nullptr, "No handler entry for source {}", *src);
			Debug::Invariant(entry->handler != nullptr,
			                 "Handler not registered for source {}",
			                 *src);

			bool ret = entry->handler(entry->handlerData);
			// After handling the interrupt we should intr_complete(), but
			// here we wake up the thread that actually handles the
			// interrupt and use compart call to let it complete the intr.
			return ret;
		}

		private:
		static bool ethernet_intr_handler(void *ctx)
		{
			uint32_t retBits;
			auto [rv, shouldYield] =
			  static_cast<Event *>(ctx)->bits_set(&retBits, 0x1);

			Debug::Assert(
			  rv == 0,
			  "Setting bits for ethernet interrupt handler failed {}",
			  rv);

			return shouldYield;
		}

		/**
		 * The reserved space for the master PLIC. In theory there could be
		 * multiple PLICs in the system and we can easily have multiple PLIC
		 * instances in addition to master, although for the MCU we will
		 * probably only ever have just the one.
		 */
		static char masterPlic[];

		void intr_handler_register(SourceID         src,
		                           InterruptHandler handler,
		                           void            *handlerData)
		{
			auto entry = src2handler(src);
			Debug::Invariant(
			  entry != nullptr, "No handler entry for source {}", src);
			Debug::Assert(entry->handler == nullptr,
			              "Source ID {} already registered",
			              src);

			entry->handler     = handler;
			entry->handlerData = handlerData;
		}

		void intr_handler_unregister(SourceID src)
		{
			auto entry = src2handler(src);
			Debug::Invariant(
			  entry != nullptr, "No handler entry for source {}", src);
			Debug::Assert(entry->handler != nullptr,
			              "Trying to unregister unset interrupt handler for {}",
			              src);
			entry->handler = nullptr;
		}

		struct HandlerEntry
		{
			InterruptHandler handler;
			void            *handlerData;
		} handlerTable[InterruptsNum];

		/**
		 * Convert interrupt ID into the handler entry.
		 * @return nullopt if src cannot map to a handler
		 */
		HandlerEntry *src2handler(SourceID src)
		{
			if (src == EthernetSourceId)
			{
				return &handlerTable[0];
			}
			return nullptr;
		}

		alignas(Event) char ethernetEvent[sizeof(Event)];
	};

	alignas(Plic) inline char Plic::masterPlic[sizeof(Plic)];
} // namespace sched

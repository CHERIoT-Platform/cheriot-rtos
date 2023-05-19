// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "common.h"
#include "event.h"
#include <compartment.h>
#include <optional>
#include <platform-plic.hh>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils.hh>

/*
 * Platform specific low-level implementations of the interrupt controller. Not
 * to be used directly. These should be inherited by the interrupt controller
 * wrapper class.
 */
namespace
{
	using Priority = uint32_t;
	using SourceID = uint32_t;

} // namespace

namespace sched
{
	template<typename T, size_t MaxIntrID, typename SourceID, typename Priority>
	concept IsPlic = requires(T v, SourceID id, Priority p)
	{
		{v.interrupt_enable(id)};
		{v.interrupt_disable(id)};
		{v.interrupt_disable(id)};
		{v.priority_set(id, p)};
		{
			v.interrupt_claim()
			} -> std::same_as<std::optional<SourceID>>;
		{v.interrupt_complete(id)};
	};

	/*
	 * FIXME: Sail doesn't have an interrupt controller at all, but we pretend
	 * it does just like FLUTE build to let things compile. We need tons of
	 * #ifdefs or a big rewrite to make the entire external interrupt path
	 * optional.
	 *
	 * FIXME: Here should only be platform-agnostic code but we still hardcode
	 * an Ethernet handler. We should be generic and auto-generate event
	 * channels and intr_complete() functions here.
	 */

	/**
	 * The PLIC class that wraps platform-specific implementations and
	 * provides higher-level abstractions.
	 */
	class InterruptController final
	{
		/**
		 * Structure representing the configuration for an interrupt.
		 */
		struct Interrupt
		{
			/**
			 * The interrupt number.
			 */
			uint32_t number;
			/**
			 * The priority for this interrupt.
			 */
			uint32_t priority;
		};

		/**
		 * The array of interrupts that are configured.
		 */
		static constexpr Interrupt ConfiguredInterrupts[] = {
#ifdef CHERIOT_INTERRUPT_CONFIGURATION
		  CHERIOT_INTERRUPT_CONFIGURATION
#endif
		};

		/**
		 * The number of interrupts that are configured.
		 *
		 * We only allocate state for configured interrupts.
		 */
		static constexpr size_t NumberOfInterrupts =
		  std::extent_v<decltype(ConfiguredInterrupts)>;

		static constexpr uint32_t LargestInterruptNumber = []() {
			uint32_t max = 0;
			for (auto i : ConfiguredInterrupts)
			{
				max = std::max(max, i.number);
			}
			return max;
		}();

		using PlicType = Plic<LargestInterruptNumber, SourceID, Priority>;

		static_assert(
		  IsPlic<PlicType, LargestInterruptNumber, SourceID, Priority>,
		  "Provided PLIC does not implement the required interface");
		/**
		 * The platform local interrupt controller device.
		 */
		PlicType device;

		/**
		 * The futex words corresponding to each interrupt.  Each word is
		 * incremented whenever the interrupt fires.  This allows handlers to
		 * use this in a pseudo-polling style.  We could turn this into a real
		 * polling model if we were to design a custom interrupt controller.
		 */
		uint32_t futexWords[NumberOfInterrupts];

		public:
		/**
		 * If source corresponds to a valid interrupt, return a reference to
		 * it, otherwise return an invalid value.
		 */
		utils::OptionalReference<uint32_t>
		futex_word_for_source(SourceID source)
		{
			for (size_t i = 0; i < NumberOfInterrupts; i++)
			{
				if (ConfiguredInterrupts[i].number == uint32_t(source))
				{
					return {futexWords[i]};
				}
			}
			return nullptr;
		}

		static InterruptController &master()
		{
			// masterPlic is the statically-allocated space for the main Plic,
			// and we know it's been placement-newed during boot in
			// master_init(), so it contains a valid Plic object.
			return reinterpret_cast<InterruptController &>(masterPlic);
		}

		static void master_init()
		{
			// The Clang analyser has a false positive here, not seeing the
			// `alignas` directive on masterPlic
			new (masterPlic) // NOLINT(clang-analyzer-cplusplus.PlacementNew)
			  InterruptController();
		}

		InterruptController()
		{
			// Set the priority for each interrupt and then enable it.
			for (auto interrupt : ConfiguredInterrupts)
			{
				device.priority_set(interrupt.number, interrupt.priority);
				device.interrupt_enable(interrupt.number);
			}
		}

		/**
		 * Handling external interrupts.
		 * @return true if a task is preempted and needs reschedule
		 */
		[[nodiscard]] utils::OptionalReference<uint32_t> do_external_interrupt()
		{
			// TODO: Replace this with and_then() when we update libc++ to
			// support C++23.
			std::optional<SourceID> src = device.interrupt_claim();
			if (!src)
			{
				// We entered external interrupt but for whatever reason the
				// interrupt controller says it sees nothing, so just return.
				return nullptr;
			}

			return futex_word_for_source(*src);
		}

		void interrupt_complete(SourceID id)
		{
			device.interrupt_complete(id);
		}

		private:
		/**
		 * The reserved space for the master PLIC. In theory there could be
		 * multiple PLICs in the system and we can easily have multiple PLIC
		 * instances in addition to master, although for the MCU we will
		 * probably only ever have just the one.
		 */
		static char masterPlic[];
	};

	alignas(InterruptController) inline char InterruptController::masterPlic
	  [sizeof(InterruptController)];
} // namespace sched

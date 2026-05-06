// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <optional>
#include <stdint.h>
#include <type_traits>

namespace
{

	/**
	 * Capture build time interrupt configuration
	 */
	struct InterruptConfiguration
	{
		/**
		 * Structure representing the configuration for an interrupt, as
		 * provided by sdk/xmake.lua
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
			/**
			 * True if this interrupt is edge triggered, false otherwise.  Edge
			 * triggered interrupts are automatically acknowledged, level
			 * triggered interrupts must be explicitly acknowledged.
			 */
			bool isEdgeTriggered;
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

		std::optional<uint32_t> index_of_source(uint32_t source)
		{
			for (size_t i = 0; i < NumberOfInterrupts; i++)
			{
				if (ConfiguredInterrupts[i].number == source)
				{
					return {i};
				}
			}
			return std::nullopt;
		}

		uint32_t source_of_index(uint32_t index)
		{
			return ConfiguredInterrupts[index].number;
		}
	};

} // namespace

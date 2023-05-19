// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cheri.hh>
#include <compartment-macros.h>
#include <optional>
#include <stdint.h>
#include <utils.hh>

/**
 * Driver for the standard RISC-V Platform-Local Interrupt Controller (PLIC).
 *
 * `MaxIntrID` is the largest interrupt number that is used.  `SourceID` and
 * `Priority` are the types used for interrupt source numbers and priorities,
 * respectively.
 */
template<size_t MaxIntrID, typename SourceID, typename Priority>
class StandardPlic : private utils::NoCopyNoMove
{
	public:
	/**
	 * Constructor.  Initialises the interrupt controller with all interrupts
	 * disabled.
	 */
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

	/**
	 * Enable the specified interrupt.
	 */
	void interrupt_enable(SourceID src)
	{
		size_t idx    = src >> 5;
		size_t bitPos = src & 0x1f;

		uint32_t enables = plicEnables[idx] | (1U << bitPos);
		plicEnables[idx] = enables;
	}

	/**
	 * Disable the specified interrupt.
	 */
	void interrupt_disable(SourceID src)
	{
		size_t idx    = src >> 5;
		size_t bitPos = src & 0x1f;

		uint32_t enables = plicEnables[idx] & ~(1U << bitPos);
		plicEnables[idx] = enables;
	}

	/**
	 * Set the priority of the specified interrupt.
	 */
	void priority_set(SourceID src, Priority prio)
	{
		plicPrios[src] = prio;
	}

	/**
	 * Fetch the interrupt number that fired and prevent it from firing.  If
	 * two or more interrupts have fired then this will return the
	 * highest-priority one.
	 *
	 * If no interrupt has fired (for example, because the interrupt line on
	 * the core was raised spuriously) then this returns `stdd:nullopt`.
	 */
	std::optional<SourceID> interrupt_claim()
	{
		uint32_t claim = *plicClaim;
		// PLIC reserves source ID 0, which means no interrupts.
		return claim == 0 ? std::nullopt : std::optional{claim};
	}

	/**
	 * Tell the interrupt controller we've handled a specified interrupt ID.
	 */
	void interrupt_complete(SourceID src)
	{
		*plicClaim = src;
	}

	private:
	// generic offsets according to spec
	static constexpr size_t PriorityOffset  = 0x0U;
	static constexpr size_t PendingOffset   = 0x1000U;
	static constexpr size_t EnableOffset    = 0x2000U;
	static constexpr size_t ThresholdOffset = 0x200000U;
	static constexpr size_t ClaimOffset     = 0x200004U;

	// Bounded capabilities to the individual structure fields.
	// Ideally these should be contained in one volatile struct, but
	// they are so far apart (and this lets us apply the bounds once).
	volatile uint32_t *plicPrios;
	volatile uint32_t *plicPendings;
	volatile uint32_t *plicEnables;
	volatile uint32_t *plicThres;
	volatile uint32_t *plicClaim;
};

/**
 * Type representing no interrupt controller.
 *
 * Contains stub implementations of all of the methods.
 */
template<size_t MaxIntrID, typename SourceID, typename Priority>
class NoPlic : private utils::NoCopyNoMove
{
	public:
	void interrupt_enable(SourceID) {}
	void interrupt_disable(SourceID) {}

	void priority_set(SourceID, Priority) {}

	std::optional<SourceID> interrupt_claim()
	{
		return std::nullopt;
	}
	void interrupt_complete(SourceID) {}
};

/**
 * The type for the Programmable Local Interrupt Controller (PLIC) to use.
 *
 * If there is no plic device then this provides a stub version.
 */
template<size_t MaxIntrID, typename SourceID, typename Priority>
using Plic =
#if DEVICE_EXISTS(plic)
  StandardPlic
#else
  NoPlic
#endif
  <MaxIntrID, SourceID, Priority>;

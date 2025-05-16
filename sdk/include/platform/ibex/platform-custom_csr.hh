// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

/*
 * Ibex defines some custom M-mode CSRs to control some of its extensions:
 *
 * - cpuctrlsts: "CPU Control and Status Register"
 * - secureseed: "Security Feature Seed Register"
 *
 * See https://ibex-core.readthedocs.io/en/latest/03_reference/cs_registers.html
 */

#include <bitpacks.hh>
#include <stdint.h>

namespace IbexCSR
{
	static constexpr uint32_t CpuctrlstsIndex = 0x7C0;

	struct Cpuctrlsts : Bitpack<uint32_t>
	{
		BITPACK_USUAL_PREFIX;

		/// Enable the instruction cache, if present in RTL
		BITPACK_DEFINE_TYPE_ENUM_CLASS(Icache, uint8_t, 0, 0) {
			Disable = 0,
			Enable  = 1
		};

		/// Enable data-independent timing featuers, if RTL has set SecureIbex
		BITPACK_DEFINE_TYPE_ENUM_CLASS(DataIndependentTiming, uint8_t, 1, 1) {
			Disable = 0,
			Enable  = 1
		};

		/// Dispatch dummy instructions, if RTL has set SecureIbex
		BITPACK_DEFINE_TYPE_ENUM_CLASS(DataInstructionInsertion,
		                               uint8_t,
		                               2,
		                               2) {
			Disable = 0,
			Enable  = 1
		};

		/// Frequency of dummy instruction insertion, if RTL has set SecureIbex
		BITPACK_DEFINE_TYPE_NUMERIC(DummyInstructionRate, uint8_t, 3, 5);

		/**
		 * The core has taken a synchronous exception and not yet retired a
		 * mret. May be manually set or cleared for advanced handling.
		 */
		BITPACK_DEFINE_TYPE_ENUM_CLASS(SynchronousException, uint8_t, 6, 6) {
			Inactive = 0,
			Active   = 1
		};

		/**
		 * The core took a synchonous exception while SynchronousExeptionSeen
		 * was set.  Hardware never clears this bit.
		 */
		BITPACK_DEFINE_TYPE_ENUM_CLASS(DoubleFault, uint8_t, 7, 7) {
			Unseen = 0,
			Seen   = 1
		};
	};

	static constexpr uint32_t SecureseedIndex = 0x7C1;

} // namespace IbexCSR

#pragma once

#include <concepts>

struct Timeout;

namespace Revocation
{
	/**
	 * Concept for a hardware revoker.  Boards can provide their own definition
	 * of this, which must be found in `<hardware_revoker.hh>` in a path
	 * provided by the board search.
	 */
	template<typename T>
	concept IsHardwareRevokerDevice = requires(T v, uint32_t epoch) {
		{ v.init() };
		{ v.system_epoch_get() } -> std::same_as<uint32_t>;
		{
			v.template has_revocation_finished_for_epoch<true>(epoch)
		} -> std::same_as<uint32_t>;
		{
			v.template has_revocation_finished_for_epoch<false>(epoch)
		} -> std::same_as<uint32_t>;
		{ v.system_bg_revoker_kick() } -> std::same_as<void>;
	};

	/**
	 * If this revoker supports an interrupt to notify of completion then it
	 * must have a method that blocks waiting for the interrupt to fire.  This
	 * method should return true if the epoch has been reached or false if the
	 * timeout expired.
	 */
	template<typename T>
	concept SupportsInterruptNotification =
	  requires(T v, Timeout *timeout, uint32_t epoch) {
		  { v.wait_for_completion(timeout, epoch) } -> std::same_as<bool>;
	  };
} // namespace Revocation

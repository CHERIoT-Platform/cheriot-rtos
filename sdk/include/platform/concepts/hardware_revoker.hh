#pragma once

#include <cheri.hh>
#include <concepts>

struct Timeout;

namespace Revocation
{
	/**
	 * Concept for a revoker.  Boards can provide their own definition
	 * of this, which must be found in `<platform-hardware_revoker.hh>` in a
	 * path provided by the board include directories.
	 */
	template<typename T>
	concept IsRevokerDevice =
	  requires(T v) {
		  { T::IsAsynchronous } -> std::same_as<const bool &>;

		  { v.init() } -> std::same_as<void>;

		  { v.system_epoch_get() } -> std::same_as<uint32_t>;

		  { v.system_bg_revoker_kick() } -> std::same_as<void>;
	  } &&
	  requires(T v, uint32_t epoch) {
		  {
			  v.template has_revocation_finished_for_epoch<true>(epoch)
		  } -> std::same_as<uint32_t>;
		  {
			  v.template has_revocation_finished_for_epoch<false>(epoch)
		  } -> std::same_as<uint32_t>;
	  } &&
	  requires(T                        v,
	           ptraddr_t                addr,
	           ptraddr_t                endAddr,
	           CHERI::Capability<void> *cap) {
		  // Read the revocation state of a particular address
		  { v.mark_get(addr) } -> std::same_as<bool>;

		  /*
		   * Mark cap.address as revoked.
		   *
		   * Marks created with this method do not need immediate enforcement:
		   * the allocator uses this only to mark the locations of its in-band
		   * headers and will ensure that capabilities to these addresses do not
		   * leak out of its compartment.  That is, these marks must be visible
		   * to subequent `mark_get` and mark manipulation after this function
		   * returns, but the allocator's object state machine intends that
		   * there are no pointers in the system that will be impacted by this
		   * mark.
		   */
		  { v.mark_set_one(cap) } -> std::same_as<void>;

		  { v.mark_clear_one(addr) } -> std::same_as<void>;

		  /*
		   * Set a range of addresses, [cap.base(), cap.top()), as revoked.
		   *
		   * This is invoked for each object the allocator frees for reuse.  The
		   * allocator's object lifecycle state machine ensures that these marks
		   * will remain at least through the next epoch opening and closing.
		   *
		   * Marks created by this method must be immediately enforced
		   * everywhere, except on the calling core, for which the requirement
		   * is merely that the core, and in particular its load barrier,
		   * percieve these marks.  Subsequent `mark_get` and mark manipulation
		   * must also percieve these marks after this function returns.
		   *
		   * That is, unlike with `mark_set_one`, there may be pointers in the
		   * system that will be impacted by these marks, and locations holding
		   * capabilities, other than main memory and the current core's
		   * register file (see below), must ensure that copies of any such will
		   * be found and invalidated before the sooner of...
		   *
		   * 1. the next attempt use or propagate such a capability or
		   *
		   * 2. the closing of the first epoch opened after this call returns
		   *    (at which point, the allocator is free to clear these marks).
		   *
		   * Main memory is allowed relaxed percepton because the load barrier
		   * serves to prevent propagaition (and use) of capabilities affected
		   * by these marks and because the latter deadline will necessarily be
		   * met by the revoker's scan of memory.
		   *
		   * The calling core is allowed relaxed perception because it is, by
		   * virtue of calling this function, presumed to be exeucing
		   * `heap_free()` within the allocator compartment and so will exit via
		   * the switcher's cross-call return path.  The net effect is that the
		   * register file will be sufficiently refreshed by reads from memory
		   * during that return -- if not already done by preemption -- that
		   * there is no need to forcibly do so now.
		   */
		  { v.mark_set_range(cap) } -> std::same_as<void>;

		  { v.mark_clear_range(addr, endAddr) } -> std::same_as<void>;
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

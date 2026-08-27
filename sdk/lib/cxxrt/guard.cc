// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include "guard.h"
#include <atomic>
#include <cdefs.h>
#include <debug.hh>
#include <locks.hh>
#include <stdint.h>

using Debug = ConditionalDebug<DEBUG_CXXRT, "cxxrt">;

namespace
{
	/**
	 * Helper for operating on the guard word. The guard word is a 64-bit value
	 * where the low bit indicates that the variable is initialised and the
	 * high bit indicates that it's locked.
	 */
	class GuardWord
	{
		/**
		 * The low half (first on a little-endian system).
		 *
		 * This is the word that matters for the ABI: The compiler will treat
		 * the low bit of this being set as an indication that the value is
		 * initialised.
		 */
		std::atomic<uint32_t> low;
		/// The high half (second on a little-endian system).
		FlagLockPriorityInherited high;

		public:
		/**
		 * Returns true if this guard is initialised.
		 */
		bool is_initialised()
		{
			return low & 1;
		}

		/**
		 * Sets that this guard is initialised.
		 */
		void set_initialised()
		{
			low = 1;
		}

		/**
		 * Acquire the lock.  Returns a lock guard that holds the lock until
		 * destroyed.
		 */
		auto aquire_lock()
		{
			return LockGuard{high};
		}

		/**
		 * Explicitly release the lock.
		 */
		void unlock()
		{
			high.unlock();
		}

		/**
		 * Check that the lock is owned by us (in debug mode);
		 */
		__always_inline void assert_owned()
		{
			if constexpr (DEBUG_CXXRT)
			{
				Debug::Assert(high.get_owner_thread_id() == thread_id_get(),
				              "Lock is owned by thread {}, not us (thread {})",
				              high.get_owner_thread_id(),
				              thread_id_get());
			}
		}
	};
} // namespace

/**
 * Acquire the lock for a guard word associated with a static.  Returns 0 if
 * the variable has already been initialised, 1 otherwise.  If this returns 1,
 * then it has acquired the lock, which must be released with
 * `__cxa_guard_release`.
 */
int __cxa_guard_acquire(uint64_t *guard)
{
	auto *guardWord = reinterpret_cast<GuardWord *>(guard);
	auto  g         = guardWord->aquire_lock();
	if (guardWord->is_initialised())
	{
		return 0;
	}
	g.release();
	return 1;
}

/**
 * Release a guard word and mark the variable as initialised.
 */
void __cxa_guard_release(uint64_t *guard)
{
	auto *g = reinterpret_cast<GuardWord *>(guard);
	Debug::Assert(
	  !g->is_initialised(), "Releasing already-initialized guard {}", g);
	g->assert_owned();
	g->set_initialised();
	g->unlock();
}

/**
 * Register a global destructor.  We have no notion of program end and so this
 * does nothing.
 */
int __cxa_atexit(void (*)(void *), void *, void *)
{
	return 0;
}

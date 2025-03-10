// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <debug.hh>
#include <futex.h>
#include <limits>
#include <stdint.h>

using Debug = ConditionalDebug<DEBUG_CXXRT, "cxxrt">;

/**
 * The helper functions need to expose an unmangled name because the compiler
 * inserts calls to them.  Declare them using the asm label extension.
 */
#define DECLARE_ATOMIC_LIBCALL(name, ret, ...)                                 \
	[[cheriot::interrupt_state(disabled)]] __cheri_libcall ret name(           \
	  __VA_ARGS__) asm(#name);

DECLARE_ATOMIC_LIBCALL(__cxa_guard_acquire, int, uint64_t *)
DECLARE_ATOMIC_LIBCALL(__cxa_guard_release, void, uint64_t *)
DECLARE_ATOMIC_LIBCALL(__cxa_atexit, int, void (*)(void *), void *, void *)

namespace
{
	/**
	 * Helper for operating on the guard word. The guard word is a 64-bit value
	 * where the low bit indicates that the variable is initialised and the
	 * high bit indicates that it's locked.
	 */
	class GuardWord
	{
		/// The low half (first on a little-endian system).
		uint32_t low;
		/// The high half (second on a little-endian system).
		uint32_t high;
		/// The bit used for the lock (the high bit on a little-endian system)
		static constexpr uint32_t LockBit = static_cast<uint32_t>(1) << 31;

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
		 * Acquire the lock.
		 *
		 * This is safe only in IRQ-deferred context.
		 */
		void lock()
		{
			// Block until the lock word is 0, then set it.
			while (high & LockBit)
			{
				futex_wait(&high, LockBit);
			}
			Debug::Assert(high == 0, "Corrupt guard word at {}", this);
			high = LockBit;
		}

		/**
		 * Release the lock
		 */
		void unlock()
		{
			Debug::Assert(high == LockBit, "Corrupt guard word at {}", this);
			high    = 0;
			int res = futex_wake(&high, std::numeric_limits<uint32_t>::max());
			Debug::Assert(res >= 0,
			              "futex_wake failed for guard {}; possible deadlock",
			              this);
		}

		/**
		 * Returns true if the lock is held.
		 */
		bool is_locked()
		{
			return high == LockBit;
		}
	};
} // namespace

/**
 * Acquire the lock for a guard word associated with a static.  Returns 0 if
 * the variable has already been initialised, 1 otherwise.  If this returns 1,
 * then it has acquired the lock, which must be released with
 * `__cxa_guard_release`.
 */
// NOLINTNEXT(readability-identifier-naming)
int __cxa_guard_acquire(uint64_t *guard)
{
	auto *g = reinterpret_cast<GuardWord *>(guard);
	if (g->is_initialised())
	{
		return 0;
	}
	g->lock();
	return 1;
}

/**
 * Release a guard word and mark the variable as initialised.
 */
// NOLINTNEXT(readability-identifier-naming)
void __cxa_guard_release(uint64_t *guard)
{
	auto *g = reinterpret_cast<GuardWord *>(guard);
	Debug::Assert(!g->is_initialised(), "Releasing uninitialized guard {}", g);
	Debug::Assert(g->is_locked(), "Releasing unlocked guard {}", g);
	g->set_initialised();
	g->unlock();
}

/**
 * Register a global destructor.  We have no notion of program end and so this
 * does nothing.
 */
// NOLINTNEXT(readability-identifier-naming)
int __cxa_atexit(void (*)(void *), void *, void *)
{
	return 0;
}

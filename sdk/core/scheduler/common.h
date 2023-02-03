// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <cdefs.h>
#include <cheri.hh>
#include <debug.hh>
#include <stdlib.h>
#include <type_traits>

template<size_t NFrames>
struct TrustedStackGeneric;

using TrustedStack = TrustedStackGeneric<0>;

namespace sched
{
	constexpr bool DebugScheduler =
#ifdef DEBUG_SCHEDULER
	  DEBUG_SCHEDULER
#else
	  false
#endif
	  ;

	using Debug = ConditionalDebug<DebugScheduler, "Scheduler">;
	/**
	 * Base class for types that are exported from the scheduler with a common
	 * sealing type.  Includes an inline type marker.
	 */
	struct Handle
	{
		protected:
		/**
		 * The real type of this subclass.
		 */
		enum class Type : uint8_t
		{
			/**
			 * Scheduler queue object.
			 */
			Queue,

			/**
			 * Scheduler event channel type.
			 */
			Event,

			/**
			 * Multiwaiter type.
			 */
			MultiWaiter
		} type;

		/**
		 * Constructor, takes the type of the subclass.
		 */
		Handle(Type type) : type(type) {}

		public:
		/**
		 * Unseal `unsafePointer` as a pointer to an object of the specified
		 * type.  Returns nullptr if `unsafePointer` is not a valid sealed
		 * pointer to an object of the correct type.
		 */
		template<typename T>
		static T *unseal(void *unsafePointer)
		{
			return static_cast<Handle *>(unsafePointer)->unseal_as<T>();
		}
		/**
		 * Unseal this object as the specified type.  Returns nullptr if this
		 * is not a valid sealed object of the correct type.
		 */
		template<typename T>
		T *unseal_as()
		{
			static_assert(std::is_base_of_v<Handle, T>,
			              "Cannot down-cast something that is not a subclass "
			              "of Handle");
			auto unsealed = compart_unseal(this);
			if (unsealed.is_valid() && unsealed->type == T::TypeMarker)
			{
				return unsealed.cast<T>();
			}
			return nullptr;
		}
	};
	/**
	 * Info about a thread to be passed from loader to the scheduler. The
	 * scheduler will take this record and initialise the thread block.
	 */
	struct ThreadLoaderInfo
	{
		/// The trusted stack for this thread. This field should be sealed by
		/// the loader and contain populated PCC, CGP and CSP caps.
		TrustedStack *trustedStack;
		/// Thread ID. Certain compartments need to know which thread it is in.
		uint16_t threadid;
		/// Thread priority. The higher the more prioritised.
		uint16_t priority;
	};

	/**
	 * RAII class for preventing nested exceptions.
	 */
	struct ExceptionGuard
	{
		/// The number of exception's currently being handled.
		static inline uint8_t exceptionLevel;

		/// RAII type, no copy constructor.
		ExceptionGuard(const ExceptionGuard &) = delete;
		/// RAII type, no move constructor.
		ExceptionGuard(ExceptionGuard &&) = delete;

		/**
		 * Constructor.  Increments the exception level and calls the handler
		 * if we are in a nested exception.
		 */
		__always_inline ExceptionGuard(auto &&errorHandler)
		{
			exceptionLevel++;
			if (exceptionLevel > 1)
			{
				errorHandler();
			}
		}

		/**
		 * Destructor, decrements the current exception level.
		 */
		__always_inline ~ExceptionGuard()
		{
			exceptionLevel--;
		}

		/**
		 * Panic if we are in a trying to block in an interrupt handler.
		 *
		 * In debug builds, this will report an error message with the caller's
		 * source location.
		 */
		static void
		assert_safe_to_block(SourceLocation loc = SourceLocation::current())
		{
			Debug::Invariant<>(exceptionLevel == 0,
			                   "Trying to block in an interrupt context.",
			                   loc);
		}
	};

	__BEGIN_DECLS
	void exception_entry_asm(void);
	__END_DECLS

} // namespace sched

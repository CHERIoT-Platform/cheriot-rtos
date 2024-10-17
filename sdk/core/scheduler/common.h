// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include "loaderinfo.h"
#include <cdefs.h>
#include <cheri.hh>
#include <debug.hh>
#include <stdlib.h>
#include <token.h>
#include <type_traits>

namespace
{
	constexpr bool DebugScheduler =
#ifdef DEBUG_SCHEDULER
	  DEBUG_SCHEDULER
#else
	  false
#endif
	  ;

	/// Is scheduler accounting enabled?
	constexpr bool Accounting =
#ifdef SCHEDULER_ACCOUNTING
	  SCHEDULER_ACCOUNTING
#else
	  false
#endif
	  ;

	using Debug = ConditionalDebug<DebugScheduler, "Scheduler">;

	constexpr StackCheckMode StackMode =
#if CHERIOT_STACK_CHECKS_SCHEDULER
	  StackCheckMode::Asserting
#else
	  StackCheckMode::Disabled
	// Uncomment if checks failed to find the correct values
	// StackCheckMode::Logging
#endif
	  ;

#define STACK_CHECK(expected)                                                  \
	StackUsageCheck<StackMode, expected, __PRETTY_FUNCTION__> stackCheck

	/**
	 * Base class for sealed objects that are exported from the scheduler.
	 *
	 * Subclasses must implement a static `sealing_type` method that returns
	 * the sealing key.
	 */
	template<bool IsDynamic>
	struct Handle
	{
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
			void *result;
			if constexpr (IsDynamic)
			{
				result = token_obj_unseal_dynamic(T::sealing_type(),
				                                  reinterpret_cast<SObj>(this));
			}
			else
			{
				result = token_obj_unseal_static(T::sealing_type(),
				                                 reinterpret_cast<SObj>(this));
			}
			return static_cast<T *>(result);
		}
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

} // namespace

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <__debug.h>
#include <cheri.hh>
#include <compartment.h>
#include <concepts>
#include <cstddef>
#include <platform-uart.hh>
#include <string.h>
#include <switcher.h>

#include <array>
#include <string_view>
#include <type_traits>

namespace DebugConcepts
{
	/// Helper concept for matching booleans
	template<typename T>
	concept IsBool = std::is_same_v<T, bool>;

	/// Helper concept for matching enumerations.
	template<typename T>
	concept IsEnum = std::is_enum_v<T>;

	/// Concept for something that can be lazily called to produce a bool.
	template<typename T>
	concept LazyAssertion = requires(T v) {
		{
			v()
		} -> IsBool;
	};

	template<typename T>
	concept IsPointerButNotCString =
	  std::is_pointer_v<T> && !std::is_same_v<T, const char *>;

	template<typename T>
	concept IsConvertibleToAddress =
	  std::convertible_to<T, ptraddr_t> && !IsEnum<T>;

} // namespace DebugConcepts

/**
 * Abstract class for writing debug output.  This is used for custom output.
 *
 * This may be changed in the future to provide better support for custom
 * formatting.
 */
struct DebugWriter
{
	/**
	 * Write a single character.
	 */
	virtual void write(char) = 0;
	/**
	 * Write a C string.
	 */
	virtual void write(const char *) = 0;
	/**
	 * Write a string view.
	 */
	virtual void write(std::string_view) = 0;
	/**
	 * Write a 32-bit unsigned integer.
	 */
	virtual void write(uint32_t) = 0;
	/**
	 * Write a 32-bit signed integer.
	 */
	virtual void write(int32_t) = 0;
	/**
	 * Write a 64-bit unsigned integer.
	 */
	virtual void write(uint64_t) = 0;
	/**
	 * Write a 64-bit signed integer.
	 */
	virtual void write(int64_t) = 0;
};

/**
 * Helper function for writing enumerations.  Enumerations are written using
 * magic_enum to provide a string and then a numeric value.
 */
template<typename T>
void debug_enum_helper(uintptr_t value, DebugWriter &writer)
    requires DebugConcepts::IsEnum<T>
{
	writer.write(magic_enum::enum_name<T>(static_cast<T>(value)));
	writer.write('(');
	writer.write(static_cast<uint32_t>(value));
	writer.write(')');
}

/**
 * Callback for custom types in debug output.  This should use the second
 * argument to write the first argument to the debug output.
 */
using DebugCallback = void (*)(uintptr_t, DebugWriter &);

/**
 * Adaptor that turns an argument of type `T` into a `DebugFormatArgument`.
 *
 * Users may specialise these to provide custom formatters.  See the
 * specialisation for enumerations for an example.
 */
template<typename T>
struct DebugFormatArgumentAdaptor;

/**
 * Boolean specialisation, prints "true" or "false".
 */
template<>
struct DebugFormatArgumentAdaptor<bool>
{
	__always_inline static DebugFormatArgument construct(bool value)
	{
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentBool};
	}
};

/**
 * Character specialisation, prints the character.
 */
template<>
struct DebugFormatArgumentAdaptor<char>
{
	__always_inline static DebugFormatArgument construct(char value)
	{
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentCharacter};
	}
};

/**
 * Unsigned character specialisation, prints the character as a hex number.
 */
template<>
struct DebugFormatArgumentAdaptor<uint8_t>
{
	__always_inline static DebugFormatArgument construct(uint8_t value)
	{
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentUnsignedNumber32};
	}
};

/**
 * Unsigned 16-bit integer specialisation, prints the integer as a hex number.
 */
template<>
struct DebugFormatArgumentAdaptor<uint16_t>
{
	__always_inline static DebugFormatArgument construct(uint16_t value)
	{
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentUnsignedNumber32};
	}
};

/**
 * Unsigned 32-bit integer specialisation, prints the integer as a hex number.
 */
template<>
struct DebugFormatArgumentAdaptor<uint32_t>
{
	__always_inline static DebugFormatArgument construct(uint32_t value)
	{
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentUnsignedNumber32};
	}
};

/**
 * Unsigned 64-bit integer specialisation, prints the integer as a hex number.
 *
 * All smaller sizes are handled by zero extending to 32 bits.  We treat 64-bit
 * separately because it requires decomposing the two halves for printing and
 * that's redundant overhead for the majority of cases.
 */
template<>
struct DebugFormatArgumentAdaptor<uint64_t>
{
	__always_inline static DebugFormatArgument construct(uint64_t value)
	{
		uintptr_t fudgedValue;
		memcpy(&fudgedValue, &value, sizeof(fudgedValue));
		return {fudgedValue,
		        DebugFormatArgumentKind::DebugFormatArgumentUnsignedNumber64};
	}
};

/**
 * Signed 8-bit integer specialisations, print the integer as a decimal number.
 */
template<>
struct DebugFormatArgumentAdaptor<int8_t>
{
	__always_inline static DebugFormatArgument construct(int8_t value)
	{
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentSignedNumber32};
	}
};

/**
 * Signed 16-bit integer specialisations, print the integer as a decimal number.
 */
template<>
struct DebugFormatArgumentAdaptor<int16_t>
{
	__always_inline static DebugFormatArgument construct(int16_t value)
	{
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentSignedNumber32};
	}
};

/**
 * Signed 32-bit integer specialisations, print the integer as a decimal number.
 */
template<>
struct DebugFormatArgumentAdaptor<int32_t>
{
	__always_inline static DebugFormatArgument construct(int32_t value)
	{
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentSignedNumber32};
	}
};

/**
 * Signed 64-bit integer specialisations, print the integer as a decimal number.
 *
 * All smaller sizes are handled by sign extending to 32 bits.  We treat 64-bit
 * separately because it requires 64-bit division to convert a 64-bit integer
 * to a decimal and that, in turn, requires libcalls.
 */
template<>
struct DebugFormatArgumentAdaptor<int64_t>
{
	__always_inline static DebugFormatArgument construct(int64_t value)
	{
		static_assert(sizeof(uintptr_t) == sizeof(uint64_t));
		uintptr_t fudgedValue;
		memcpy(&fudgedValue, &value, sizeof(fudgedValue));
		return {fudgedValue,
		        DebugFormatArgumentKind::DebugFormatArgumentSignedNumber64};
	}
};

/**
 * C string specialisation, prints the string as-is.
 */
template<>
struct DebugFormatArgumentAdaptor<const char *>
{
	__always_inline static DebugFormatArgument construct(const char *value)
	{
		return {reinterpret_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentCString};
	}
};

/**
 * String view specialisation, prints the string as-is.
 *
 * Note that this relies on the string view persisting for the duration of the
 * call.  It passes a pointer to the string-view argument.
 */
template<>
struct DebugFormatArgumentAdaptor<std::string_view>
{
	__always_inline static DebugFormatArgument
	construct(std::string_view &value)
	{
		return {reinterpret_cast<uintptr_t>(&value),
		        DebugFormatArgumentKind::DebugFormatArgumentStringView};
	}
};

/**
 * String view specialisation, use the C string handler.
 */
template<>
struct DebugFormatArgumentAdaptor<std::string>
{
	__always_inline static DebugFormatArgument construct(std::string &value)
	{
		return DebugFormatArgumentAdaptor<const char *>::construct(
		  value.c_str());
	}
};

/**
 * Enum specialisation, prints the enum as a string and then the numeric value.
 *
 * This specialisation uses the generic printing facility in the library call
 * and passes a callback that will map the enumeration to a string.
 */
template<DebugConcepts::IsEnum T>
struct DebugFormatArgumentAdaptor<T>
{
	__always_inline static DebugFormatArgument construct(T value)
	{
#ifdef CHERIOT_AVOID_CAPRELOCS
		return {static_cast<uintptr_t>(value),
		        DebugFormatArgumentKind::DebugFormatArgumentUnsignedNumber32};
#else
		return {static_cast<uintptr_t>(value),
		        reinterpret_cast<uintptr_t>(&debug_enum_helper<T>)};
#endif
	}
};

/**
 * Permission set specialisation.
 */
template<>
struct DebugFormatArgumentAdaptor<CHERI::PermissionSet>
{
	__always_inline static DebugFormatArgument
	construct(CHERI::PermissionSet value)
	{
		return {static_cast<uintptr_t>(value.as_raw()),
		        DebugFormatArgumentKind::DebugFormatArgumentPermissionSet};
	}
};

/**
 * Null pointer specialisation.
 */
template<>
struct DebugFormatArgumentAdaptor<std::nullptr_t>
{
	__always_inline static DebugFormatArgument construct(std::nullptr_t)
	{
		return {0, DebugFormatArgumentKind::DebugFormatArgumentPointer};
	}
};

/**
 * Pointer specialisation, prints the pointer as a full capability.
 */
template<DebugConcepts::IsPointerButNotCString T>
struct DebugFormatArgumentAdaptor<T>
{
	__always_inline static DebugFormatArgument construct(T value)
	{
		return {reinterpret_cast<uintptr_t>(
		          static_cast<const volatile void *>(value)),
		        DebugFormatArgumentKind::DebugFormatArgumentPointer};
	}
};

/**
 * Specialisation for the CHERI capability wrapper class, prints the capability
 * in the same format as bare pointers.
 */
template<typename T>
struct DebugFormatArgumentAdaptor<CHERI::Capability<T>>
{
	__always_inline static DebugFormatArgument
	construct(CHERI::Capability<T> value)
	{
		return {reinterpret_cast<uintptr_t>(
		          static_cast<const volatile void *>(value)),
		        DebugFormatArgumentKind::DebugFormatArgumentPointer};
	}
};

/**
 * Specialisation for things that can be converted to addresses, prints as an
 * unsigned number.
 */
template<DebugConcepts::IsConvertibleToAddress T>
struct DebugFormatArgumentAdaptor<T>
{
	__always_inline static DebugFormatArgument construct(ptraddr_t value)
	{
		return {static_cast<uintptr_t>(value),

		        DebugFormatArgumentKind::DebugFormatArgumentUnsignedNumber32};
	}
};

/**
 * Recursive helper that maps from a tuple representing the arguments into a
 * type-erased array.
 */
template<size_t I>
__always_inline inline void map_debug_argument(DebugFormatArgument *arguments,
                                               auto &&argumentTuple)
{
	arguments[I] =
	  DebugFormatArgumentAdaptor<
	    std::remove_cvref_t<decltype(std::get<I>(argumentTuple))>>{}
	    .construct(std::get<I>(argumentTuple));
	if constexpr (I > 0)
	{
		map_debug_argument<I - 1>(arguments, argumentTuple);
	}
}

/**
 * Convert `args` into a type-erased array of `DebugFormatArgument`s in
 * `arguments`.
 */
template<typename... Args>
__always_inline inline void
make_debug_arguments_list(DebugFormatArgument *arguments, Args &...args)
{
	if constexpr (sizeof...(Args) > 0)
	{
		map_debug_argument<sizeof...(Args) - 1>(arguments,
		                                        std::forward_as_tuple(args...));
	}
}

/**
 * Library function that writes a debug message.  This runs with interrupts
 * disabled (to avoid interleaving) and prints an array of debug messages. This
 * is intended to allow a single call to print multiple format strings without
 * requiring the format strings to be copied, so that the debugging APIs can
 * wrap a user-provided format string.
 */
__cheri_libcall void debug_log_message_write(const char          *context,
                                             const char          *format,
                                             DebugFormatArgument *messages,
                                             size_t               messageCount);

__cheri_libcall void debug_report_failure(const char          *kind,
                                          const char          *file,
                                          const char          *function,
                                          int                  line,
                                          const char          *fmt,
                                          DebugFormatArgument *arguments,
                                          size_t               argumentCount);

namespace
{
	/**
	 * Helper class wrapping a string for use as a template argument.  This is
	 * used to describe a context for conditional debugging that will be
	 * prefixed to debug lines.
	 */
	template<size_t N>
	struct DebugContext
	{
		/**
		 * Constructor, captures the string argument.
		 */
		constexpr DebugContext(const char (&str)[N])
		{
			std::copy_n(str, N, value);
		}

		/**
		 * Implicit conversion to a C string.
		 */
		constexpr operator const char *() const
		{
			return value;
		}

		/**
		 * The captured string.  Must be public for this class to meet the
		 * requirements of a structural type for use as a template argument.
		 */
		char value[N];
	};

	/**
	 * Our libc++ does not currently provide source_location, but our clang
	 * provides the necessary builtins for one.
	 */
	struct SourceLocation
	{
		/**
		 * Explicitly construct a source location.
		 */
		constexpr SourceLocation(int         lineNumber,
		                         int         columnNumber,
		                         const char *fileName,
		                         const char *functionName)
		  : lineNumber(lineNumber),
		    columnNumber(columnNumber),
		    fileName(fileName),
		    functionName(functionName)
		{
		}

		/**
		 * Construct a source location for the caller.
		 */
		static constexpr SourceLocation __always_inline
		current(int         lineNumber   = __builtin_LINE(),
		        int         columnNumber = __builtin_COLUMN(),
		        const char *fileName     = __builtin_FILE(),
		        const char *functionName = __builtin_FUNCTION()) noexcept
		{
			return {lineNumber, columnNumber, fileName, functionName};
		}

		/// Returns the line number for this source location.
		[[nodiscard]] __always_inline constexpr int line() const noexcept
		{
			return lineNumber;
		}
		/// Returns the column number for this source location.
		[[nodiscard]] __always_inline constexpr int column() const noexcept
		{
			return columnNumber;
		}
		/// Returns the file name for this source location.
		[[nodiscard]] __always_inline constexpr const char *
		file_name() const noexcept
		{
			return fileName;
		}
		/// Returns the function name for this source location.
		[[nodiscard]] __always_inline constexpr const char *
		function_name() const noexcept
		{
			return functionName;
		}

		private:
		/// The line number of this source location.
		int lineNumber;
		/// The column number of this source location.
		int columnNumber;
		/// The file name of this source location.
		const char *fileName;
		/// The function name of this source location.
		const char *functionName;
	};

	/**
	 * Conditional debug class.  Used to control conditional output and
	 * assertion checking.  Enables debug log messages and assertions if
	 * `Enabled` is true.  Uses `Context` to print additional detail on debug
	 * lines.  Each line is prefixed with the context string in magenta to make
	 * it easy to see debug output from different subsystems in the same trace.
	 *
	 * This class is expected to be used as a type alias, something like:
	 *
	 * ```c++
	 * constexpr bool DebugFoo = DEBUG_FOO;
	 * using Debug = ConditionalDebug<DebugFoo, "Foo">;
	 * ```
	 */
	template<bool Enabled, DebugContext Context>
	class ConditionalDebug
	{
		public:
		/**
		 * Log a message.
		 *
		 * This function does nothing if the `Enabled` condition is false.
		 */
		template<typename... Args>
		static void log(const char *fmt, Args... args)
		{
			if constexpr (Enabled)
			{
				asm volatile("" ::: "memory");
				DebugFormatArgument arguments[sizeof...(Args)];
				make_debug_arguments_list(arguments, args...);
				const char *context = Context;
				debug_log_message_write(
				  context, fmt, arguments, sizeof...(Args));
				asm volatile("" ::: "memory");
			}
		}

		/**
		 * Helper to report failure.
		 *
		 * This must not take the `SourceLocation` directly because doing so
		 * prevents the compiler from decomposing and subsequently
		 * constant-propagating its fields in the caller, resulting in 24 bytes
		 * of stack space consumed for every assert or invariant.
		 */
		template<typename... Args>
		static inline void report_failure(const char *kind,
		                                  const char *file,
		                                  const char *function,
		                                  int         line,
		                                  const char *fmt,
		                                  Args... args)
		{
			// Ensure that the compiler does not reorder messages.
			DebugFormatArgument arguments[sizeof...(Args)];
			make_debug_arguments_list(arguments, args...);
			const char *context = Context;
			debug_report_failure(
			  kind, file, function, line, fmt, arguments, sizeof...(Args));
		}

		/**
		 * Function-like class for invariants that is expected to be used via
		 * the deduction guide as:
		 *
		 * ConditionalDebug::Invariant(someCondition, "A message...", ...);
		 *
		 * Invariants are checked unconditionally but will log a verbose
		 * message only if `Enabled` is true.
		 */
		template<typename... Args>
		struct Invariant
		{
			/**
			 * Constructor, performs the invariant check.
			 */
			__always_inline
			Invariant(bool        condition,
			          const char *fmt,
			          Args... args,
			          SourceLocation loc = SourceLocation::current())
			{
				if (__predict_false(!condition))
				{
					if constexpr (Enabled)
					{
						report_failure("Invariant",
						               loc.file_name(),
						               loc.function_name(),
						               loc.line(),
						               fmt,
						               std::forward<Args>(args)...);
					}
					__builtin_trap();
				}
			}
		};
		/**
		 * Function-like class for assertions that is expected to be used via
		 * the deduction guide as:
		 *
		 * ConditionalDebug::Assert(someCondition, "A message...", ...);
		 *
		 * Assertions are checked only if `Enabled` is true.
		 */
		template<typename... Args>
		struct Assert
		{
			/**
			 * Constructor, performs the assertion check.
			 */
			template<typename T>
			    requires DebugConcepts::IsBool<T>
			__always_inline
			Assert(T           condition,
			       const char *fmt,
			       Args... args,
			       SourceLocation loc = SourceLocation::current())
			{
				if constexpr (Enabled)
				{
					if (__predict_false(!condition))
					{
						report_failure("Assertion",
						               loc.file_name(),
						               loc.function_name(),
						               loc.line(),
						               fmt,
						               std::forward<Args>(args)...);
						__builtin_trap();
					}
				}
			}

			/**
			 * Constructor, performs an assertion check.
			 *
			 * This version is passed a lambda that returns a bool, rather than
			 * a boolean condition.  This allows the compiler to completely
			 * elide the contents of the lambda in builds where this component
			 * is not being debugged.  This version should be used for places
			 * where the assertion condition has side effects.
			 */
			template<typename T>
			    requires DebugConcepts::LazyAssertion<T>
			__always_inline
			Assert(T         &&condition,
			       const char *fmt,
			       Args... args,
			       SourceLocation loc = SourceLocation::current())
			{
				if constexpr (Enabled)
				{
					if (__predict_false(!condition()))
					{
						report_failure("Assertion",
						               loc.file_name(),
						               loc.function_name(),
						               loc.line(),
						               fmt,
						               std::forward<Args>(args)...);
						__builtin_trap();
					}
				}
			}
		};

		/**
		 * Deduction guide, allows `Invariant` to be used as if it were a
		 * function.
		 */
		template<typename T, typename... Ts>
		Invariant(T, const char *, Ts &&...) -> Invariant<Ts...>;

		/**
		 * Overt wrapper function around Invariant.  Sometimes template
		 * deduction guides just don't cut it.  At the cost of a
		 * std::make_tuple at the call site, we can still take advantage
		 * of much of the machinery here.
		 */
		template<typename T, typename... Args>
		__always_inline static void
		invariant(T                   condition,
		          const char         *fmt,
		          std::tuple<Args...> args = std::make_tuple(),
		          SourceLocation      loc  = SourceLocation::current())
		{
			std::apply(
			  [&](Args... iargs) {
				  Invariant<Args...>(
				    condition, fmt, std::forward<Args>(iargs)..., loc);
			  },
			  args);
		}

		/**
		 * Deduction guide, allows `Assert` to be used as if it were a
		 * function.
		 */
		template<typename... Ts>
		Assert(bool, const char *, Ts &&...) -> Assert<Ts...>;

		/**
		 * Deduction guide, allows `Assert` to be used as if it were a
		 * function with a lambda argument.
		 */
		template<typename... Ts>
		Assert(auto, const char *, Ts &&...) -> Assert<Ts...>;

		/**
		 * Overt wrapper function around Assert.  Sometimes template
		 * deduction guides just don't cut it.  At the cost of a
		 * std::make_tuple at the call site, we can still take advantage
		 * of much of the machinery here.
		 */
		template<typename T, typename... Args>
		__always_inline static void
		assertion(T                   condition,
		          const char         *fmt,
		          std::tuple<Args...> args = std::make_tuple(),
		          SourceLocation      loc  = SourceLocation::current())
		{
			std::apply(
			  [&](Args... iargs) {
				  Assert<Args...>(
				    condition, fmt, std::forward<Args>(iargs)..., loc);
			  },
			  args);
		}
	};

	enum class StackCheckMode
	{
		Disabled,
		Logging,
		Asserting,
	};

	/**
	 * Check the (dynamic) stack usage of a function, including all of its
	 * callees.  This is intended to be used in compartment entry points to
	 * check the stack size that is required for that entry point.  Use this
	 * with some test vectors that explore different code paths to identify the
	 * maximum stack usage.  This can then be used with the
	 * [[cheriot::minimum_stack]] attribute to ensure that the switcher will
	 * never invoke the entry point with insufficient stack.
	 *
	 * Note that this records only the stack usage for the current compartment
	 * invocation (including any invoked shared libraries).  If this
	 * compartment calls others, then a larger stack may be required.  This
	 * failure is recoverable: cross-compartment calls should always be assumed
	 * to potentially fail.  This check is to ensure that an attacker cannot
	 * cause stack overflow to crash the compartment, not to ensure that an
	 * attacker cannot cause stack overflow to make the operation that they
	 * requested fail.
	 *
	 * Stack checks run in one of three modes:
	 *
	 *  - Disabled: The checks do not run.
	 *  - Logging: The checks run and print a message if the stack usage
	 *    exceeds expectations.
	 *  - Asserting: After printing the message, the function will trap.
	 *
	 * In logging mode, one message will be printed for each invocation of the
	 * function that exceeds the previous maximum stack usage.
	 *
	 * An instance of this should be created as a local variable on entry to a
	 * function.  The destructor (which prints the message) will then run after
	 * any other destructors, ensuring the most accurate stack usage
	 * measurement.
	 *
	 * Unfortunately, default arguments for templates are not evaluated in the
	 * enclosing scope and so `__builtin_FUNCTION()` cannot be used here.
	 * Instead, we must pass this explicitly, typically as something like:
	 *
	 * ```c++
	 * StackUsageCheck<DebugFlag, 128, __PRETTY_FUNCTION__> stackCheck;
	 * ```
	 */
	template<StackCheckMode Mode, size_t Expected, DebugContext Fn>
	class StackUsageCheck
	{
		/**
		 * The expected maximum.  This class is templated on the function name
		 * and so there is one copy of this per function.
		 */
		static inline size_t stackUsage = Expected;

		public:
		/**
		 * Default constructor, does nothing.
		 */
		StackUsageCheck() = default;

		/**
		 * Destructor, runs at the end of the function to print the message.
		 */
		__always_inline ~StackUsageCheck()
		{
			if constexpr (Mode != StackCheckMode::Disabled)
			{
				ptraddr_t lowest = stack_lowest_used_address();
				ptraddr_t highest =
				  CHERI::Capability{__builtin_cheri_stack_get()}.top();
				size_t used = highest - lowest;
				if (used > stackUsage)
				{
					stackUsage = used;
					ConditionalDebug<true, Fn>::log("Stack used: {} bytes",
					                                stackUsage);
					if constexpr (Mode == StackCheckMode::Asserting)
					{
						__builtin_trap();
					}
				}
			}
		}
	};

} // namespace

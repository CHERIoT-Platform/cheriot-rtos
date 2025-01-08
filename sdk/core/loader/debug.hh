// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once
#include <cheri.hh>
#include <compartment.h>
#include <platform-uart.hh>

#include <array>

namespace
{
	/**
	 * Is the loader being debugged?
	 */
	static constexpr bool DebugLoader = DEBUG_LOADER;

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
	} // namespace DebugConcepts

	/**
	 * Helper class for writing debug output to the UART.  This performs no
	 * internal buffering and assumes interrupts are disabled for the duration
	 * to avoid interference.
	 *
	 * This is based on snmalloc's class of the same name.
	 */
	class LoaderWriter
	{
		/**
		 * The capability to the UART device.
		 */
		static inline volatile Uart *uart16550;

		/**
		 * Append a character, delegating to `Output`.
		 */
		void append_char(char c)
		{
			uart16550->blocking_write(c);
		}

		/**
		 * Append a null-terminated C string.
		 */
		void append(const char *str)
		{
			for (; *str; ++str)
			{
				append_char(*str);
			}
		}

		/**
		 * Append a string view.
		 */
		void append(std::string_view str)
		{
			for (char c : str)
			{
				append_char(c);
			}
		}

		void append(char c)
		{
			append_char(c);
		}

		/**
		 * Outputs the permission set using the format G RWcgml Xa SU0 as
		 * described in [/docs/Debugging.md].
		 */
		void append(CHERI::PermissionSet permissions)
		{
			using namespace CHERI;
			auto perm = [&](Permission p, char c) -> char {
				if (permissions.contains(p))
				{
					return c;
				}
				return '-';
			};
			format("{} {}{}{}{}{}{} {}{} {}{}{}",
			       perm(Permission::Global, 'G'),
			       perm(Permission::Load, 'R'),
			       perm(Permission::Store, 'W'),
			       perm(Permission::LoadStoreCapability, 'c'),
			       perm(Permission::LoadGlobal, 'g'),
			       perm(Permission::LoadMutable, 'm'),
			       perm(Permission::StoreLocal, 'l'),
			       perm(Permission::Execute, 'X'),
			       perm(Permission::AccessSystemRegisters, 'a'),
			       perm(Permission::Seal, 'S'),
			       perm(Permission::Unseal, 'U'),
			       perm(Permission::User0, '0'));
		}

		/**
		 * Append a raw pointer as a hex string.
		 */
		__noinline void append(const void *ptr)
		{
			const CHERI::Capability C{ptr};

			format("{} (v:{} {}-{} l:{} o:{} p: {})",
			       C.address(),
			       C.is_valid(),
			       C.base(),
			       C.top(),
			       C.length(),
			       C.type(),
			       C.permissions());
		}

		/**
		 * Append a capability.
		 */
		template<typename T>
		__always_inline void append(CHERI::Capability<T> capability)
		{
			append(static_cast<const void *>(capability.get()));
		}

		/**
		 * Append a signed integer, as a decimal string.
		 */
		__noinline void append(int32_t s)
		{
			if (s < 0)
			{
				append_char('-');
				s = 0 - s;
			}
			std::array<char, 10> buf;
			const char           Digits[] = "0123456789";
			for (int i = int(buf.size() - 1); i >= 0; i--)
			{
				buf.at(static_cast<size_t>(i)) = Digits[s % 10];
				s /= 10;
			}
			bool skipZero = true;
			for (auto c : buf)
			{
				if (skipZero && (c == '0'))
				{
					continue;
				}
				skipZero = false;
				append_char(c);
			}
			if (skipZero)
			{
				append_char('0');
			}
		}

		/**
		 * Append a 32-bit unsigned integer to the buffer as hex with no prefix.
		 */
		__attribute__((noinline)) void append_hex_word(uint32_t s)
		{
			std::array<char, 8> buf;
			const char          Hexdigits[] = "0123456789abcdef";
			// Length of string including null terminator
			static_assert(sizeof(Hexdigits) == 0x11);
			for (long i = long(buf.size() - 1); i >= 0; i--)
			{
				buf.at(static_cast<size_t>(i)) = Hexdigits[s & 0xf];
				s >>= 4;
			}
			bool skipZero = true;
			for (auto c : buf)
			{
				if (skipZero && (c == '0'))
				{
					continue;
				}
				skipZero = false;
				append_char(c);
			}
			if (skipZero)
			{
				append_char('0');
			}
		}

		/**
		 * Append a 32-bit unsigned integer to the buffer as hex.
		 */
		__attribute__((noinline)) void append(uint32_t s)
		{
			append_char('0');
			append_char('x');
			append_hex_word(s);
		}

		/**
		 * Append a 64-bit unsigned integer to the buffer as hex.
		 */
		__attribute__((noinline)) void append(uint64_t s)
		{
			append_char('0');
			append_char('x');
			uint32_t hi = static_cast<uint32_t>(s >> 32);
			uint32_t lo = static_cast<uint32_t>(s);
			if (hi != 0)
			{
				append_hex_word(hi);
			}
			append_hex_word(lo);
		}

		/**
		 * Append a 16-bit unsigned integer to the buffer as hex.
		 */
		__always_inline void append(uint16_t s)
		{
			append(static_cast<uint32_t>(s));
		}

		/**
		 * Append an 8-bit unsigned integer to the buffer as hex.
		 */
		__always_inline void append(uint8_t s)
		{
			append(static_cast<uint32_t>(s));
		}

		/**
		 * Append an enumerated type value.
		 */
		template<typename T>
		    requires DebugConcepts::IsEnum<T>
		void append(T e)
		{
			// `magic_enum::enum_name` requires cap relocs, so don't use it in
			// components that want or need to avoid them.
			append(static_cast<int32_t>(e));
		}

		public:
		/**
		 * Base case for formatting: no format-string arguments.
		 */
		void format(const char *fmt)
		{
			append(fmt);
		}

		/**
		 * Inductive case for formatting.  Writes up to the instruction to
		 * output the first argument, writes that argument, and then recurses.
		 */
		template<typename T, typename... Args>
		void format(const char *fmt, T firstArg, Args... args)
		{
			for (const char *s = fmt; *s != 0; ++s)
			{
				if (s[0] == '{' && s[1] == '}')
				{
					append(firstArg);
					return format(s + 2, args...);
				}

				append_char(*s);
			}
		}

		/**
		 * Set the UART memory pointer.
		 */
		static void set_uart(volatile Uart *theUART)
		{
			uart16550 = theUART;
		}
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
	 * lines.  Writes output using `Writer`.
	 *
	 * If `DisableInterrupts` is true then this disables interrupts while
	 * printing the message to avoid accidental interleaving.
	 *
	 * This class is expected to be used as a type alias, something like:
	 *
	 * ```c++
	 * constexpr bool DebugFoo = DEBUG_FOO;
	 * using Debug = ConditionalDebug<DebugFoo, "Foo">;
	 * ```
	 */
	template<bool Enabled>
	class LoaderDebug
	{
		public:
		/// The (singleton) writer used for output.
		static inline LoaderWriter writer;

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
				// Ensure that the compiler does not reorder messages.
				asm volatile("" ::: "memory");
				writer.format("\x1b[35mloader\033[0m");
				writer.format(": ");
				writer.format(fmt, args...);
				writer.format("\n");
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
		__noinline static void report_failure(const char *kind,
		                                      const char *file,
		                                      const char *function,
		                                      int         line,
		                                      const char *fmt,
		                                      Args... args)
		{
			// Ensure that the compiler does not reorder messages.
			asm volatile("" ::: "memory");
			writer.format(
			  "\x1b[35m{}:{} \x1b[31m{} failure\x1b[35m in {}\x1b[36m\n",
			  file,
			  line,
			  kind,
			  function);
			writer.format(fmt, args...);
			writer.format("\033[0m\n");
			asm volatile("" ::: "memory");
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
				if (!condition)
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
					if (!condition)
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
					if (!condition())
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
	};

	/**
	 * Debug interface for the loader.  Enables verbose debugging if
	 * `DebugLoader` is true and requires that the loader explicitly
	 * initialises the UART.
	 */
	using Debug = LoaderDebug<DebugLoader>;
} // namespace

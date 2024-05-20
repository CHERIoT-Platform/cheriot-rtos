// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <debug.hh>

using namespace CHERI;

namespace
{
	/**
	 * Printer for debug messages.  This implements the `DebugWriter` interface
	 * so that it can be used with custom callbacks.
	 *
	 * This is not exposed for subclassing and so is final to allow internal
	 * calls to avoid the vtable.  Only callbacks use the vtable.
	 */
	struct DebugPrinter final : DebugWriter
	{
		/**
		 * Write a character, delegating to `Output`.
		 */
		void write(char c) override
		{
			MMIO_CAPABILITY(Uart, uart)->blocking_write(c);
		}

		/**
		 * Write a null-terminated C string.
		 */
		void write(const char *str) override
		{
			for (; *str; ++str)
			{
				write(*str);
			}
		}

		/**
		 * Write a string view.
		 */
		void write(std::string_view str) override
		{
			for (char c : str)
			{
				write(c);
			}
		}

		/**
		 * Outputs the permission set using the format G RWcgml Xa SU0 as
		 * described in [/docs/Debugging.md].
		 */
		void write(CHERI::PermissionSet permissions)
		{
			using namespace CHERI;
			auto perm = [&](Permission p, char c) -> char {
				if (permissions.contains(p))
				{
					return c;
				}
				return '-';
			};
			write(perm(Permission::Global, 'G'));
			write(' ');
			write(perm(Permission::Load, 'R'));
			write(perm(Permission::Store, 'W'));
			write(perm(Permission::LoadStoreCapability, 'c'));
			write(perm(Permission::LoadGlobal, 'g'));
			write(perm(Permission::LoadMutable, 'm'));
			write(perm(Permission::StoreLocal, 'l'));
			write(' ');
			write(perm(Permission::Execute, 'X'));
			write(perm(Permission::AccessSystemRegisters, 'a'));
			write(' ');
			write(perm(Permission::Seal, 'S'));
			write(perm(Permission::Unseal, 'U'));
			write(perm(Permission::User0, '0'));
		}

		/**
		 * Write a raw pointer as a hex string.
		 */
		void write(void *ptr)
		{
			const CHERI::Capability C{ptr};

			write(C.address());
			write(" (v:");
			write(C.is_valid());
			write(' ');
			write(C.base());
			write('-');
			write(C.top());
			write(" l:");
			write(C.length());
			write(" o:");
			write(C.type());
			write(" p: ");
			write(C.permissions());
			write(')');
		}

		/**
		 * Write a signed integer, as a decimal string.
		 */
		void write(int32_t s) override
		{
			if (s < 0)
			{
				write('-');
				s = 0 - s;
			}
			std::array<char, 10> buf;
			const char           Digits[] = "0123456789";
			for (int i = int(buf.size() - 1); i >= 0; i--)
			{
				buf[static_cast<size_t>(i)] = Digits[s % 10];
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
				write(c);
			}
			if (skipZero)
			{
				write('0');
			}
		}

		/**
		 * Write a signed integer, as a decimal string.
		 */
		void write(int64_t s) override
		{
			if (s < 0)
			{
				write('-');
				s = 0 - s;
			}
			std::array<char, 20> buf;
			const char           Digits[] = "0123456789";
			for (int i = int(buf.size() - 1); i >= 0; i--)
			{
				buf[static_cast<size_t>(i)] = Digits[s % 10];
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
				write(c);
			}
			if (skipZero)
			{
				write('0');
			}
		}

		/**
		 * Write a 32-bit unsigned integer to the buffer as hex with no prefix.
		 */
		void append_hex_word(uint32_t s)
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
				write(c);
			}
			if (skipZero)
			{
				write('0');
			}
		}

		/**
		 * Write a 32-bit unsigned integer to the buffer as hex.
		 */
		void write(uint32_t s) override
		{
			write('0');
			write('x');
			append_hex_word(s);
		}

		/**
		 * Write a 64-bit unsigned integer to the buffer as hex.
		 */
		void write(uint64_t s) override
		{
			write('0');
			write('x');
			uint32_t hi = static_cast<uint32_t>(s >> 32);
			uint32_t lo = static_cast<uint32_t>(s);
			if (hi != 0)
			{
				append_hex_word(hi);
			}
			append_hex_word(lo);
		}

		/**
		 * Format a message, using the provided arguments.
		 */
		void format(const char          *fmt,
		            DebugFormatArgument *arguments,
		            size_t               argumentsCount)
		{
			// If there are no format arguments, just write the string.
			if (argumentsCount == 0)
			{
				write(fmt);
				return;
			}
			size_t argumentIndex = 0;
			for (const char *s = fmt; *s != 0; ++s)
			{
				if (s[0] == '{' && s[1] == '}')
				{
					s++;
					if (argumentIndex >= argumentsCount)
					{
						write("<missing argument>");
						continue;
					}
					auto            &argument = arguments[argumentIndex++];
					Capability<void> kind{
					  reinterpret_cast<void *>(argument.kind)};
					if (kind.is_valid())
					{
						reinterpret_cast<DebugCallback>(kind.get())(
						  argument.value, *this);
					}
					else
					{
						switch (
						  static_cast<DebugFormatArgumentKind>(argument.kind))
						{
							case DebugFormatArgumentKind::
							  DebugFormatArgumentBool:
								write(static_cast<bool>(argument.value)
								        ? "true"
								        : "false");
								break;
							case DebugFormatArgumentKind::
							  DebugFormatArgumentCharacter:
								write(static_cast<char>(argument.value));
								break;
							case DebugFormatArgumentKind::
							  DebugFormatArgumentPointer:
								write(reinterpret_cast<void *>(argument.value));
								break;
							case DebugFormatArgumentKind::
							  DebugFormatArgumentSignedNumber32:
								write(static_cast<int32_t>(argument.value));
								break;
							case DebugFormatArgumentKind::
							  DebugFormatArgumentUnsignedNumber32:
								write(static_cast<uint32_t>(argument.value));
								break;
							case DebugFormatArgumentKind::
							  DebugFormatArgumentSignedNumber64:
							{
								int64_t value;
								memcpy(&value, &argument.value, sizeof(value));
								write(value);
								break;
							}
							case DebugFormatArgumentKind::
							  DebugFormatArgumentUnsignedNumber64:
							{
								uint64_t value;
								memcpy(&value, &argument.value, sizeof(value));
								write(value);
								break;
							}
							case DebugFormatArgumentKind::
							  DebugFormatArgumentCString:
								write(reinterpret_cast<const char *>(
								  argument.value));
								break;
							case DebugFormatArgumentKind::
							  DebugFormatArgumentStringView:
								write(*reinterpret_cast<std::string_view *>(
								  argument.value));
								break;
							case DebugFormatArgumentKind::
							  DebugFormatArgumentPermissionSet:
								write(CHERI::PermissionSet::from_raw(
								  argument.value));
								break;
							default:
								write("<invalid argument kind>");
								break;
						}
					};
					continue;
				}
				write(*s);
			}
		}
	};

} // namespace

[[cheri::interrupt_state(disabled)]] void
debug_log_message_write(const char          *context,
                        const char          *format,
                        DebugFormatArgument *messages,
                        size_t               messageCount)
{
	DebugPrinter printer;
	printer.write("\x1b[35m");
	printer.write(context);
	printer.write("\033[0m: ");
	printer.format(format, messages, messageCount);
	printer.write("\n");
}

[[cheri::interrupt_state(disabled)]] void
debug_report_failure(const char          *kind,
                     const char          *file,
                     const char          *function,
                     int                  line,
                     const char          *format,
                     DebugFormatArgument *arguments,
                     size_t               argumentCount)
{
	DebugPrinter printer;
	printer.write("\x1b[35m");
	printer.write(file);
	printer.write(":");
	printer.write(line);
	printer.write("\x1b[31m ");
	printer.write(kind);
	printer.write(" failure\x1b[35m in ");
	printer.write(function);
	printer.write("\x1b[36m\n");
	printer.format(format, arguments, argumentCount);
	printer.write("\n");
}

#pragma once
#include <compartment-macros.h>
#include <stddef.h>
#include <stdint.h>

/**
 * The kind of value, for values that have special-cased handling.
 */
enum DebugFormatArgumentKind : ptraddr_t
{
	/// Boolean, printed as "true" or "false".
	DebugFormatArgumentBool,
	/// Single character.
	DebugFormatArgumentCharacter,
	/// Signed 32-bit integer, printed as decimal.
	DebugFormatArgumentSignedNumber32,
	/// Unsigned 32-bit integer, printed as hexadecimal
	DebugFormatArgumentUnsignedNumber32,
	/// Signed 64-bit integer, printed as decimal.
	DebugFormatArgumentSignedNumber64,
	/// Unsigned 64-bit integer, printed as hexadecimal.
	DebugFormatArgumentUnsignedNumber64,
	/// 32-bit floating-point value, printed as decimal.
	DebugFormatArgumentFloat,
	/// 64-bit floating-point value, printed as decimal.
	DebugFormatArgumentDouble,
	/// Pointer, printed as a full capability.
	DebugFormatArgumentPointer,
	/// Special case for permission sets, printed as in the capability
	/// format.
	DebugFormatArgumentPermissionSet,
	/// C string, printed as-is.
	DebugFormatArgumentCString,
	/// String view, printed as-is.
	DebugFormatArgumentStringView,
};

struct DebugFormatArgument
{
	/**
	 * The value that is being written.
	 */
	uintptr_t value;
	/**
	 * The kind of value that is being written.  This is either a pointer
	 * to a `DebugCallback` or one of the `Kind` enumeration, depending on
	 * whether the tag is set or not.
	 */
	uintptr_t kind;
};

/**
 * Library function that writes a debug message.  This runs with interrupts
 * disabled (to avoid interleaving) and prints an array of debug messages. This
 * is intended to allow a single call to print multiple format strings without
 * requiring the format strings to be copied, so that the debugging APIs can
 * wrap a user-provided format string.
 */
__cheri_libcall void
debug_log_message_write(const char                 *context,
                        const char                 *format,
                        struct DebugFormatArgument *messages,
                        size_t                      messageCount);

/**
 * Helper to write a debug message reporting an assertion or invariant failure.
 * This should be used only with the helper macros / templates in `debug.h[h]`.
 * This takes the kind of failure (for example, assert or invariant), the file,
 * function, and line number where the failure occurred, a format string, and
 * an array of arguments to the format string.
 */
__cheri_libcall void debug_report_failure(const char                 *kind,
                                          const char                 *file,
                                          const char                 *function,
                                          int                         line,
                                          const char                 *fmt,
                                          struct DebugFormatArgument *arguments,
                                          size_t argumentCount);

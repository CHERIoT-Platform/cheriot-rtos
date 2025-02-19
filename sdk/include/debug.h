#pragma once
#include <__debug.h>
#include <__macro_map.h>

#ifndef __cplusplus

/**
 * Helper macro to convert the type of an argument to the corresponding
 * `DebugFormatArgument` value.
 *
 * Should not be used directly.
 */
#	define CHERIOT_DEBUG_MAP_ARGUMENT(x)                                      \
		{(uintptr_t)(x),                                                       \
		 _Generic((x),                                                         \
		 _Bool: DebugFormatArgumentBool,                                       \
		 char: DebugFormatArgumentCharacter,                                   \
		 short: DebugFormatArgumentSignedNumber32,                             \
		 unsigned short: DebugFormatArgumentUnsignedNumber32,                  \
		 int: DebugFormatArgumentSignedNumber32,                               \
		 unsigned int: DebugFormatArgumentUnsignedNumber32,                    \
		 signed long long: DebugFormatArgumentSignedNumber64,                  \
		 unsigned long long: DebugFormatArgumentUnsignedNumber64,              \
		 char *: DebugFormatArgumentCString,                                   \
		 default: DebugFormatArgumentPointer)}

/**
 * Helper to map a list of arguments to an initialiser for a
 * `DebugFormatArgument` array.
 *
 * Should not be used directly.
 */
#	define CHERIOT_DEBUG_MAP_ARGUMENTS(...)                                   \
		CHERIOT_MAP_LIST(CHERIOT_DEBUG_MAP_ARGUMENT, __VA_ARGS__)

/**
 * Macro that logs a message.  The `context` argument is a string that is
 * printed in magenta at the start of the line, followed by the format string.
 * Each format argument is referenced with {} in the format string and is
 * inserted in the output with the default rendering for that type.
 */
#	define CHERIOT_DEBUG_LOG(context, msg, ...)                               \
		do                                                                     \
		{                                                                      \
			struct DebugFormatArgument args[] = {                              \
			  __VA_OPT__(CHERIOT_DEBUG_MAP_ARGUMENTS(__VA_ARGS__))};           \
			debug_log_message_write(                                           \
			  context,                                                         \
			  msg,                                                             \
			  args,                                                            \
			  0 __VA_OPT__(+(sizeof(args) / sizeof(args[0]))));                \
		} while (0)

/**
 * Assert that `condition` is true, printing a message and aborting the
 * compartment invocation if not.
 */
#	define CHERIOT_INVARIANT(condition, msg, ...)                             \
		do                                                                     \
		{                                                                      \
			if (!(condition))                                                  \
			{                                                                  \
				struct DebugFormatArgument args[] = {                          \
				  __VA_OPT__(CHERIOT_DEBUG_MAP_ARGUMENTS(__VA_ARGS__))};       \
				debug_report_failure(                                          \
				  "Invariant",                                                 \
				  __FILE__,                                                    \
				  __func__,                                                    \
				  __LINE__,                                                    \
				  msg,                                                         \
				  args,                                                        \
				  0 __VA_OPT__(+(sizeof(args) / sizeof(args[0]))));            \
				__builtin_trap();                                              \
			}                                                                  \
		} while (0)

#else
#	include <debug.hh>

namespace
{
	template<typename... Args>
	__always_inline void
	cheriot_debug_log(const char *context, const char *msg, Args... args)
	{
		DebugFormatArgument arguments[sizeof...(Args)];
		make_debug_arguments_list(arguments, args...);
		debug_log_message_write(context, msg, arguments, sizeof...(Args));
	}

	template<typename... Args>
	__always_inline void cheriot_invariant(bool        condition,
	                                       const char *file,
	                                       const char *function,
	                                       int         line,
	                                       const char *msg,
	                                       Args... args)
	{
		if (!condition)
		{
			DebugFormatArgument arguments[sizeof...(Args)];
			make_debug_arguments_list(arguments, args...);
			debug_report_failure("Invariant",
			                     file,
			                     function,
			                     line,
			                     msg,
			                     arguments,
			                     sizeof...(Args));
		}
	}
} // namespace

/**
 * C++ version of `CHERIOT_DEBUG_LOG`.  This uses the C++ helpers and so will
 * pretty-print a richer set of types than the C version.
 */
#	define CHERIOT_DEBUG_LOG(context, msg, ...)                               \
		do                                                                     \
		{                                                                      \
			cheriot_debug_log(context, msg __VA_OPT__(, ) __VA_ARGS__);        \
		} while (0)

/**
 * C++ version of `CHERIOT_INVARIANT`.  This uses the C++ helpers and so will
 * pretty-print a richer set of types than the C version.
 */
#	define CHERIOT_INVARIANT(condition, msg, ...)                             \
		do                                                                     \
		{                                                                      \
			cheriot_invariant(condition,                                       \
			                  __FILE__,                                        \
			                  __func__,                                        \
			                  __LINE__,                                        \
			                  msg __VA_OPT__(, ) __VA_ARGS__);                 \
		} while (0)

#endif

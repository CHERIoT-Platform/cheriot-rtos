#include "tests.hh"
#include <compartment.h>
#include <debug.h>

using Warn = ConditionalDebug<DebugLevel::Warning, "Debug test">;

int test_debug_cxx()
{
	unsigned char x = 'c';
	CHERIOT_DEBUG_LOG("Debug messages",
	                  "Testing C++ debug log: 42:{}, true:{}, hello world:{}, "
	                  "'c':{}, &x:{}, nullptr:{}",
	                  42,
	                  true,
	                  "hello world",
	                  'c',
	                  &x,
	                  nullptr);
	// Just test that these compile:
	CHERIOT_INVARIANT(true, "Testing C++ invariant failure: 42:{}", 42);
	CHERIOT_INVARIANT(true, "Testing C++ invariant failure");
	CHERIOT_INVARIANT(
	  true, "Testing C++ invariant failure: 42:{}", 42, 1, 3, 4, "oops");
	Warn::log<DebugLevel::Information>(
	  "This should not be printed (information)");
	Warn::log<DebugLevel::Warning>("This should be printed (warning)");
	Warn::log<DebugLevel::Error>("This should be printed (error)");
	return 0;
}

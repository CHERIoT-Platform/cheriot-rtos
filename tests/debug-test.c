#include <compartment.h>
#include <debug.h>

__cheri_compartment("debug_test") int test_debug_c()
{
	unsigned char x = 'c';
	_Bool         t = true;
	CHERIOT_DEBUG_LOG(
	  "Debug messages",
	  "Testing C debug log: 42:{}, true:{}, hello world:{}, "
	  "'c':{}, &x:{}, NULL:{}, short 3:{}, unsigned short 0xf:{}",
	  42,
	  t,
	  "hello world",
	  (char)'c',
	  &x,
	  NULL,
	  (short)3,
	  (unsigned short)0xf);
	// Just test that these compile:
	CHERIOT_INVARIANT(true, "Testing C++ invariant failure: 42:{}", 42);
	CHERIOT_INVARIANT(true, "Testing C++ invariant failure");
	CHERIOT_INVARIANT(
	  true, "Testing C++ invariant failure: 42:{}", 42, 1, 3, 4, "oops");
	return 0;
}

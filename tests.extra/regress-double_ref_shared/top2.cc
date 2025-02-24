#include "common.h"

#include <debug.hh>

using Debug = ConditionalDebug<true, "top2">;

void top2()
{
	auto ref2 = SHARED_OBJECT_WITH_PERMISSIONS(
	  struct Foo, foo, true, false, false, false);

	Debug::log("ref2: {}", ref2->bar);
}

#include "common.h"

void top1()
{
	auto ref1 =
	  SHARED_OBJECT_WITH_PERMISSIONS(struct Foo, foo, true, true, false, false);

	ref1->bar = 1;
}

int entry()
{
	top1();
	top2();

	return 0;
}

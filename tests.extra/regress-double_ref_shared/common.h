#pragma once

#include <compartment-macros.h>

struct Foo
{
	int bar;
};

void top1();
void top2();

int __cheri_compartment("top") entry();

#include "helper.h"
int __cheri_compartment("top") entry()
{
	asm volatile("cmove cra, %0; cret" : : "C"(help()));
	__builtin_unreachable();
}

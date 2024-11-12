#include "helper.h"
void __cheri_compartment("top") entry()
{
	asm volatile ("cmove cra, %0; cret" : : "C"(help()));
}

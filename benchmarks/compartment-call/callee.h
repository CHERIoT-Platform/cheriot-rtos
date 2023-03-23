#include <compartment.h>

void __cheri_compartment("callee") noop();
int __cheri_compartment("callee") noop_return();
int __cheri_compartment("callee") noop_call(int start);

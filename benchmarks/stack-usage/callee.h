#include <compartment.h>

int __cheri_compartment("callee") noop_return(size_t s);
int __cheri_compartment("callee") noop_call(int start);

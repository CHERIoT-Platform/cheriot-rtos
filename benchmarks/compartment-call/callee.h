#include <compartment.h>

#ifndef METRIC
#	define METRIC rdcycle
#endif

int __cheri_compartment("callee") noop_return_metric();
int __cheri_compartment("callee") stack_using_return_metric(size_t);

int __cheri_libcall lib_noop_return_metric();

#include <compartment.h>

#ifndef METRIC
#	define METRIC rdcycle
#endif

int __cheri_compartment("callee") noop_return_metric();
int __cheri_compartment("callee") stack_using_return_metric(size_t);

int __cheri_compartment("callee") check_pointer_return_metric(void *);
int __cheri_compartment("callee") ephemeral_claim_return_metric(void *);
int __cheri_compartment("callee") claim_release_return_metric(void *);

int __cheri_compartment("callee") callee_dereference(int *);

int __cheri_compartment("callee_ueh") callee_ueh_dereference(int *);
int __cheri_compartment("callee_seh") callee_seh_dereference(int *);

int __cheri_libcall lib_noop_return_metric();

#include <compartment.h>

#ifndef METRIC
#	define METRIC rdcycle
#endif

int __cheri_compartment("callee") callee_noop_return_metric();
int __cheri_compartment("callee") callee_stack_using_return_metric(size_t);

int __cheri_compartment("callee") callee_check_pointer_return_metric(void *);
int __cheri_compartment("callee") callee_ephemeral_claim_return_metric(void *);
int __cheri_compartment("callee") callee_claim_release_return_metric(void *);

int __cheri_compartment("callee") callee_dereference(int *);

int __cheri_compartment("callee_ueh") callee_ueh_dereference(int *);
int __cheri_compartment("callee_seh") callee_seh_dereference(int *);

int __cheri_libcall lib_noop_return_metric();

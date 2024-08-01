#include "compartment-macros.h"

int __cheri_compartment("claimant") make_claim(void *ptr);

int __cheri_compartment("claimant") show_claim();

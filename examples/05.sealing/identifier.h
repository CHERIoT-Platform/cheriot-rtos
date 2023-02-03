// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>

struct Identifier;

__cheri_compartment("identifier") Identifier *identifier_create(int value);
__cheri_compartment("identifier") int identifier_value(Identifier *identifier);
__cheri_compartment("identifier") void identifier_destroy(Identifier *);

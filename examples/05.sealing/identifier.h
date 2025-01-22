// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>

struct Identifier;

typedef CHERI_SEALED(Identifier *) SealedIdentifier;

__cheri_compartment("identifier") SealedIdentifier identifier_create(int value);
__cheri_compartment("identifier") int identifier_value(
  SealedIdentifier identifier);
__cheri_compartment("identifier") void identifier_destroy(
  SealedIdentifier identifier);

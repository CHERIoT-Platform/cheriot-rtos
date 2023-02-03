// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
__cheri_compartment("crash_recovery_inner") void *test_crash_recovery_inner(
  int);
__cheri_compartment("crash_recovery_outer") void test_crash_recovery_outer(int);

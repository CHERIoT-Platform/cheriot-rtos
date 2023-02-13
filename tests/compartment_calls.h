// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <cdefs.h>
#include <string>

__cheri_compartment("compartment_calls_inner") int compartment_call_inner(
  int x0);
__cheri_compartment(
  "compartment_calls_inner") int compartment_call_inner(int x0, int x1);
__cheri_compartment("compartment_calls_inner") int compartment_call_inner(
  int        x0,
  int        x1,
  const int *x2);
__cheri_compartment("compartment_calls_inner") int compartment_call_inner(
  int        x0,
  int        x1,
  const int *x2,
  int        x3);
__cheri_compartment("compartment_calls_inner") int compartment_call_inner(
  int        x0,
  int        x1,
  const int *x2,
  int        x3,
  const int *x4);
__cheri_compartment("compartment_calls_inner") int compartment_call_inner(
  int        x0,
  int        x1,
  const int *x2,
  int        x3,
  const int *x4,
  int        x5);
__cheri_compartment("compartment_calls_inner") int compartment_call_inner(
  int        x0,
  int        x1,
  const int *x2,
  int        x3,
  const int *x4,
  int        x5,
  int        x6);
__cheri_compartment("compartment_calls_inner") void test_incorrect_export_table(
  __cheri_callback void (*fn)());
__cheri_compartment("compartment_calls_outer") void compartment_call_outer();

constexpr int ConstantValue = 0x41414141;
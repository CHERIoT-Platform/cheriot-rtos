// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <compartment.h>
#include <token.h>

// Type that we'll use for testing.
struct TestType
{
	int value;
};

int __cheri_compartment("static_sealing_inner")
  test_static_sealed_object(Sealed<TestType>);

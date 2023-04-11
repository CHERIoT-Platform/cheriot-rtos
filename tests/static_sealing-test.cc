// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Static sealing"
#include "static_sealing.h"
#include "tests.hh"
#include <cheri.hh>

using namespace CHERI;

// Create a static sealed object that we can't access
DECLARE_AND_DEFINE_STATIC_SEALED_VALUE(TestType,
                                       static_sealing_inner,
                                       SealingType,
                                       test,
                                       {42});

void test_static_sealing()
{
	// Get a pointer to it and ask for it to be unsealed.
	Sealed<TestType> value{STATIC_SEALED_VALUE(test)};
	test_static_sealed_object(value);
}

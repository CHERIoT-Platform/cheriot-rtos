// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Static sealing (inner compartment)"
#include "static_sealing.h"
#include "tests.hh"

using namespace CHERI;

void test_static_sealed_object(Sealed<TestType> obj)
{
	// Get our static sealing key.
	SKey       key = STATIC_SEALING_TYPE(SealingType);
	Capability keyCap{key};

	debug_log("Static sealing key: {}", key);
	// Make sure the sealing key has sensible permissions
	TEST((check_pointer<PermissionSet{Permission::Seal,
	                                  Permission::Unseal,
	                                  Permission::Global,
	                                  Permission::User0}>(key, 1)),
	     "Incorrect permissions on static sealing key {}",
	     key);
	// Make sure it's in the right range.
	TEST(
	  keyCap.address() >= 16,
	  "Software sealing key has an address in the hardware-reserved range: {}",
	  keyCap.address());
	TEST(keyCap.address() < 0x10000,
	     "Software sealing key has an address too large: {}",
	     keyCap.address());
	// Make sure that it's a single sealing type
	TEST(keyCap.bounds() == 1, "Invalid bounds on {}", key);

	// Try to use it
	Capability unsealed = token_unseal(key, obj);
	debug_log("Unsealed object: {}", unsealed);
	// Make sure that the unsealed allocation is the right everything.
	TEST(unsealed->value == 42, "Unexpected value in static sealed object");
	TEST(unsealed.length() == sizeof(TestType),
	     "Incorrect length on unsealed capability {}",
	     unsealed);
	TEST((check_pointer<PermissionSet{Permission::Load,
	                                  Permission::Store,
	                                  Permission::LoadStoreCapability,
	                                  Permission::LoadMutable,
	                                  Permission::LoadGlobal,
	                                  Permission::Global}>(unsealed, 1)),
	     "Incorrect permissions on unsealed statically sealed object {}",
	     unsealed);
}

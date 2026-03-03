// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Rust"
#include "tests.hh"

extern "C" int call_rust();

int __cheri_compartment("rust_test") test_rust()
{
	debug_log("Calling rust...");
	int fromRust = call_rust();
	debug_log("int from rust: {}", fromRust);
	assert(fromRust == 1);
	return 0;
}

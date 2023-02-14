// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Compartment calls (inner compartment)"
#include "compartment_calls.h"
#include "tests.hh"
#include <cheri.hh>
#include <errno.h>
#include <tuple>

using namespace CHERI;

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	return ErrorRecoveryBehaviour::ForceUnwind;
}

void test_incorrect_export_table_with_handler(__cheri_callback void (*fn)())
{
	/*
	 * Trigger a cross-compartment call with an invalid export entry.
	 */

	debug_log(
	  "test an incorrect export table entry with error handler installed");

	fn();

	TEST(false, "Should be unreachable");
}
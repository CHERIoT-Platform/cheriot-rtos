// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Test misc APIs"
#include "tests.hh"
#include <timeout.h>

/**
 * Test timeouts.
 *
 * This test checks the following:
 *
 * - A timeout of zero would not block.
 * - `elapse` saturates values, i.e., a `remaining` value of zero will still be
 *   zero after a call to `elapse`, and an `elapsed` value of `UINT32_MAX`
 *   would still be `UINT32_MAX` after a call to `elapse`.
 * - An unlimited timeout is really unlimited, i.e., a call to `elapse` does
 *   not modify its `remaining` value, which blocks.
 */
void check_timeouts()
{
	debug_log("Test timeouts.");

	// Create a zero timeout.
	Timeout t{0};
	// Ensure that a zero timeout does not block.
	TEST(!t.may_block(), "A zero timeout should not block.");

	// Create a zero timer with maximum elapsed time.
	t = Timeout{UINT32_MAX /* elapsed */, 0 /* remaining */};
	// Ensure that a call to `elapse` saturates both `elapsed` and
	// `remaining`.
	t.elapse(42);
	TEST(t.remaining == 0,
	     "`elapse` does not saturate the `remaining` value of a zero timer.");
	TEST(t.elapsed == UINT32_MAX,
	     "`elapse` does not saturate the `elapsed` value of a zero timer.");

	// Create an unlimited timeout.
	t = Timeout{UnlimitedTimeout /* remaining */};
	// Ensure that a call to `elapse` does not modify the `remaining` value
	// of the unlimited timeout.
	t.elapse(42);
	TEST(t.remaining == UnlimitedTimeout,
	     "`elapse` alters the remaining value of an unlimited timeout.");
	// Ensure that an unlimited timeout blocks.
	TEST(t.may_block(), "An unlimited timeout should block.");
}

void test_misc()
{
	check_timeouts();
}

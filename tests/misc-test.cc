// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Test misc APIs"
#include "tests.hh"
#include <compartment-macros.h>
#include <ds/pointer.h>
#include <string.h>
#include <timeout.h>

using namespace CHERI;

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

/**
 * Test memchr.
 *
 * This test checks the following:
 *
 * - memchr finds the first occurrence of the character when it is present
 *   (test for different values, particularly the first and the last one).
 * - memchr returns NULL when the string does not contain the character (test
 *   for non-NULL terminated string).
 * - memchr does not stop at \0 characters.
 * - memchr returns NULL for 0-size pointers.
 */
void check_memchr()
{
	debug_log("Test memchr.");

	char string[] = {'C', 'H', 'E', 'R', 'R', 'I', 'E', 'S'};

	TEST(memchr(string, 'C', sizeof(string)) == &string[0],
	     "memchr must return the first occurence of the character.");
	TEST(memchr(string, 'R', sizeof(string)) == &string[3],
	     "memchr must return the first occurence of the character.");
	TEST(memchr(string, 'S', sizeof(string)) == &string[7],
	     "memchr must return the first occurence of the character.");
	TEST(memchr(string, 'X', sizeof(string)) == NULL,
	     "memchr must return NULL when a character is not present.");

	char stringWithNull[] = {'Y', 'E', 'S', '\0', 'N', 'O', '\0'};

	TEST(memchr(stringWithNull, 'N', sizeof(stringWithNull)) ==
	       &stringWithNull[4],
	     "memchr must not stop at NULL characters.");

	TEST(memchr(stringWithNull, 'N', 0) == NULL,
	     "memchr must return NULL for zero-size pointers.");
}

/**
 * Test memrchr.
 *
 * This test checks the following:
 *
 * - memrchr finds the first occurrence of the character when it is present
 *   (test for different values, particularly the first and the last one).
 * - memrchr returns NULL when the string does not contain the character (test
 *   for non-NULL terminated string).
 * - memrchr does not stop at \0 characters.
 * - memrchr returns NULL for 0-size pointers.
 */
void check_memrchr()
{
	debug_log("Test memrchr.");

	char string[] = {'C', 'H', 'E', 'R', 'R', 'I', 'O', 'T'};

	TEST(memchr(string, 'C', sizeof(string)) == &string[0],
	     "memrchr must return the first occurence of the character.");
	TEST(memrchr(string, 'R', sizeof(string)) == &string[4],
	     "memrchr must return the first occurence of the character.");
	TEST(memrchr(string, 'T', sizeof(string)) == &string[7],
	     "memrchr must return the first occurence of the character.");
	TEST(memrchr(string, 'X', sizeof(string)) == NULL,
	     "memrchr must return NULL when a character is not present.");

	char stringWithNull[] = {'F', 'U', '\0', 'B', 'A', 'R', '\0'};

	TEST(memrchr(stringWithNull, 'F', sizeof(stringWithNull)) ==
	       &stringWithNull[0],
	     "memrchr must not stop at NULL characters.");

	TEST(memrchr(stringWithNull, 'Y', 0) == NULL,
	     "memrchr must return NULL for zero-size pointers.");
}

/**
 * Test pointer utilities.
 *
 * Not comprehensive, would benefit from being expanded at some point.
 */
void check_pointer_utilities()
{
	debug_log("Test pointer utilities.");

	int                              integer        = 42;
	int                             *integerPointer = &integer;
	ds::pointer::proxy::Pointer<int> pointer{integerPointer};

	TEST((pointer == integerPointer) && (*pointer == 42),
	     "The pointer proxy does not return the value of its proxy.");

	int                              anotherInteger        = -100;
	int                             *anotherIntegerPointer = &anotherInteger;
	ds::pointer::proxy::Pointer<int> anotherPointer{anotherIntegerPointer};

	pointer = anotherPointer;

	TEST((pointer == anotherIntegerPointer) && (*pointer == -100),
	     "The pointer proxy `=` operator does not correctly set the pointer.");
}

void check_shared_object(const char      *name,
                         Capability<void> object,
                         size_t           size,
                         PermissionSet    permissions)
{
	debug_log("Checking shared object {}.", object);
	TEST(object.length() == size,
	     "Object {} is {} bytes, expected {}",
	     name,
	     object.length(),
	     size);
	TEST(object.permissions() == permissions,
	     "Object {} has permissions {}, expected {}",
	     name,
	     PermissionSet{object.permissions()},
	     permissions);
}

void check_capability_set_inexact_at_most()
{
	void *p = malloc(2048);

	debug_log("Test Capability::BoundsProxy::set_inexact_at_most with {}", p);

	{
		Capability<void> q = {p};
		q.address() += 2; // misalign

		size_t reqlen = 1024;
		q.bounds().set_inexact_at_most(reqlen);
		debug_log("Requesting 1024 at align 2 resulted in {}: {}", reqlen, q);
		TEST(reqlen < 1024, "set_inexact_at_most failed to truncate");
		TEST(q.length() == reqlen, "set_inexact_at_most failed to bound");
	}

	{
		Capability<void> q = {p};
		q.address() += 1; // misalign

		size_t reqlen = 511;
		q.bounds().set_inexact_at_most(reqlen);
		debug_log("Requesting 511 at align 1 resulted in {}: {}", reqlen, q);
		TEST(reqlen == 511, "set_inexact_at_most truncated unnecessarily");
		TEST(q.length() == reqlen, "set_inexact_at_most failed to bound");
	}

	free(p);
}

int test_misc()
{
	check_timeouts();
	check_memchr();
	check_memrchr();
	check_pointer_utilities();
	check_capability_set_inexact_at_most();
	debug_log("Testing shared objects.");
	check_shared_object("exampleK",
	                    SHARED_OBJECT(void, exampleK),
	                    1024,
	                    {Permission::Global,
	                     Permission::Load,
	                     Permission::Store,
	                     Permission::LoadStoreCapability,
	                     Permission::LoadMutable});
	check_shared_object(
	  "exampleK",
	  SHARED_OBJECT_WITH_PERMISSIONS(void, exampleK, true, true, false, false),
	  1024,
	  {Permission::Global, Permission::Load, Permission::Store});
	check_shared_object(
	  "test_word",
	  SHARED_OBJECT_WITH_PERMISSIONS(void, test_word, true, false, true, false),
	  4,
	  {Permission::Global, Permission::Load, Permission::LoadStoreCapability});
	check_shared_object("test_word",
	                    SHARED_OBJECT_WITH_PERMISSIONS(
	                      void, test_word, true, false, false, false),
	                    4,
	                    {Permission::Global, Permission::Load});
	return 0;
}

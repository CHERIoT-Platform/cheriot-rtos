// Copyright CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define TEST_NAME "Test check_pointer"
#include "tests.hh"

int object;

using namespace CHERI;

/**
 * Test the `EnforceStrictPermissions` feature of `CHERI::check_pointer`.
 *
 * This test checks the following:
 *
 * 1. When passed `EnforceStrictPermissions = false` (default behavior),
 *    `CHERI::check_pointer` does not modify the permissions and length of
 *    passed capability with a Capability argument.
 * 2. When passed `EnforceStrictPermissions = false` (default behavior),
 *    `CHERI::check_pointer` does not modify the permissions and length of
 *    passed capability with a raw capability argument.
 * 3. When passed `EnforceStrictPermissions = true`, `CHERI::check_pointer`
 *    changes permissions and length appropriately with a Capability argument.
 * 4. When passed `EnforceStrictPermissions = true`, `CHERI::check_pointer`
 *    changes permissions and length appropriately with a raw capability
 *    argument.
 * 5. For both 3. and 4., it also checks that `EnforceStrictPermissions = true`
 *    does not invalidate the capability.
 */
void check_pointer_strict_mode(int *ptr)
{
	bool       isValid;
	Capability cap{ptr};

	debug_log("Test default check_pointer (EnforceStrictPermissions = false).");
	debug_log("Test with Capability argument.");

	PermissionSet permissionsAtCreation = cap.permissions();
	auto          boundsAtCreation      = cap.bounds();

	// Check that the capability has > load permissions at creation.
	TEST(permissionsAtCreation.contains(Permission::Load, Permission::Store),
	     "Permissions of new capability do not include load/store.");

	// Check that the capability has length > 1 at creation.
	TEST(boundsAtCreation > 1,
	     "Permissions of new capability do not include load/store.");

	// We will detect if `check_pointer` changes the permissions because we
	// are calling load only and bounds = 1 here.
	TEST(check_pointer<PermissionSet{Permission::Load}>(cap, 1),
	     "Cannot check valid capability (calling with Capability object).");

	TEST(cap.permissions() == permissionsAtCreation,
	     "check_pointer reduced the permissions of passed Capability "
	     "object although EnforceStrictPermissions = false.");

	TEST(cap.bounds() == boundsAtCreation,
	     "check_pointer reduced the bounds of passed Capability "
	     "object although EnforceStrictPermissions = false.");

	debug_log("Test with raw capability argument.");

	TEST(check_pointer<PermissionSet{Permission::Load}>(ptr, 1),
	     "Cannot check valid capability (calling with raw capability).");

	Capability capShouldBeTheSame{ptr};

	TEST(capShouldBeTheSame.permissions() == permissionsAtCreation,
	     "check_pointer reduced the permissions of passed raw "
	     "capability although EnforceStrictPermissions = false.");

	TEST(capShouldBeTheSame.bounds() == boundsAtCreation,
	     "check_pointer reduced the bounds of passed Capability "
	     "capability although EnforceStrictPermissions = false.");

	debug_log("Test check_pointer with EnforceStrictPermissions = true.");
	debug_log("Test with Capability argument.");

	// We can re-use `cap` here because it was not changed.
	isValid =
	  check_pointer<PermissionSet{Permission::Load}, true, true>(cap, 1);
	TEST(isValid,
	     "Cannot check valid capability when EnforceStrictPermissions = true "
	     "(calling with Capability object).");

	// The capability should now be load-only.
	TEST(cap.permissions() == PermissionSet{Permission::Load},
	     "check_pointer did not reduce the permissions of passed Capability "
	     "object although EnforceStrictPermissions = true.");

	// The bounds should now be 1.
	TEST(cap.bounds() == 1,
	     "check_pointer did not reduce the bounds of passed Capability "
	     "object although EnforceStrictPermissions = true.");

	TEST(check_pointer<PermissionSet{Permission::Load}>(cap, 1),
	     "EnforceStrictPermissions = true breaks the capability when called "
	     "with a Capability object.");

	debug_log("Test with raw capability argument.");

	// We cannot re-use `cap` because it was changed.
	Capability cap2{ptr};
	TEST(
	  cap2.permissions() == permissionsAtCreation,
	  "Permissions of freshly created capability do not include load/store.");

	isValid =
	  check_pointer<PermissionSet{Permission::Load}, true, true>(ptr, 1);
	TEST(isValid,
	     "Cannot check valid capability when EnforceStrictPermissions = true "
	     "(calling with raw capability).");

	// We need to create a new Capability object with the updated pointer
	// to check its permissions.
	Capability cap3{ptr};
	TEST(cap3.permissions() == PermissionSet{Permission::Load},
	     "check_pointer did not reduce the permissions of passed raw "
	     "capability although EnforceStrictPermissions = true.");

	TEST(cap3.bounds() == 1,
	     "check_pointer did not reduce the bounds of passed raw "
	     "capability although EnforceStrictPermissions = true.");

	TEST(check_pointer<PermissionSet{Permission::Load}>(ptr, 1),
	     "EnforceStrictPermissions = true breaks the capability when called "
	     "with a raw capability.");
}

void test_check_pointer()
{
	check_pointer_strict_mode(&object);
}

#include "cheri.hh"
#define TEST_NAME "MMIO"
#include "tests.hh"

using namespace CHERI;

void check_permissions(Capability<volatile void> mmio, PermissionSet p)
{
	PermissionSet mmioPermissions =
	  Capability{const_cast<void *>(mmio.get())}.permissions();
	TEST(mmioPermissions == p,
	     "MMIO regions has permissions {}, expected {}",
	     mmioPermissions,
	     p);
}

int test_mmio()
{
#if !__has_attribute(cheriot_mmio)
	// XXX No permissions; the compiler misinterprets this as all permissions!
	// To mitigate this, we add a static assertion in the code this macro
	// generates to make sure that this discrepancy does not cause any trouble.
	//
	// https://github.com/CHERIoT-Platform/llvm-project/issues/349
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, false, false, false, false),
	                  {Permission::Global});
#endif

	/*
	 * The cheriot_mmio annotation does some semantic checks at compile time,
	 * rejecting meaningless combinations that cannot be expressed
	 * architecturally.  We therefore don't get a chance to probe at the
	 * architectural behavior here, but that's surely fine.
	 */
#if !__has_attribute(cheriot_mmio)
	// Bad: MC without LD or SD
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, false, true, false, false),
	                  {Permission::Global});
	// Bad: LM without MC
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, false, false, true, false),
	                  {Permission::Global});
	// Bad: LM without LD (but with MC)
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, false, true, true, false),
	                  {Permission::Global});
#endif
	// OK: SD
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, true, false, false, false),
	                  {Permission::Global, Permission::Store});
	// OK: SD and MC
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, false, true, true, false, false),
	  {Permission::Global, Permission::Store, Permission::LoadStoreCapability});
#if !__has_attribute(cheriot_mmio)
	// Bad: LM without MC (but with SD)
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, true, false, true, false),
	                  {Permission::Global, Permission::Store});
	// Bad: LM without LD (but with MC and SD)
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, false, true, true, true, false),
	  {Permission::Global, Permission::Store, Permission::LoadStoreCapability});
#endif
	// OK: LD
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, false, false, false),
	                  {Permission::Global, Permission::Load});
	// OK: LD and MC
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, true, false, true, false, false),
	  {Permission::Global, Permission::Load, Permission::LoadStoreCapability});
#if !__has_attribute(cheriot_mmio)
	// Bad: LM without MC (but with LD)
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, false, true, false),
	                  {Permission::Global, Permission::Load});
#endif
	// OK: LD, MC, and LM
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, true, true, false),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::LoadStoreCapability,
	                   Permission::LoadMutable});
	// OK: LD and SD
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, true, true, false, false, false),
	  {Permission::Global, Permission::Load, Permission::Store});
	// OK: LD, SD, and MC
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, true, true, false, false),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::Store,
	                   Permission::LoadStoreCapability});
#if !__has_attribute(cheriot_mmio)
	// Bad: LM without MC (but with LD and SD)
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, true, true, false, true, false),
	  {Permission::Global, Permission::Load, Permission::Store});
#endif
	// OK: LD, SD, MC, and LM
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, true, true, true, false),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::Store,
	                   Permission::LoadStoreCapability,
	                   Permission::LoadMutable});

	// OK: LD, MC, and LG
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, true, false, true),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::LoadStoreCapability,
	                   Permission::LoadGlobal});
	// OK: LD, MC, LM, and LG
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, true, true, true),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::LoadStoreCapability,
	                   Permission::LoadMutable,
	                   Permission::LoadGlobal});
	// OK: LD, SD, MC, LM, and LG
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, true, true, true, true),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::Store,
	                   Permission::LoadStoreCapability,
	                   Permission::LoadMutable,
	                   Permission::LoadGlobal});

	return 0;
}

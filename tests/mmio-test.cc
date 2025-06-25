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
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, false, false, false, false),
	                  {Permission::Global});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, false, true, false, false),
	                  {Permission::Global});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, false, false, true, false),
	                  {Permission::Global});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, false, true, true, false),
	                  {Permission::Global});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, true, false, false, false),
	                  {Permission::Global, Permission::Store});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, false, true, true, false, false),
	  {Permission::Global, Permission::Store, Permission::LoadStoreCapability});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, false, true, false, true, false),
	                  {Permission::Global, Permission::Store});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, false, true, true, true, false),
	  {Permission::Global, Permission::Store, Permission::LoadStoreCapability});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, false, false, false),
	                  {Permission::Global, Permission::Load});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, true, false, true, false, false),
	  {Permission::Global, Permission::Load, Permission::LoadStoreCapability});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, false, true, false),
	                  {Permission::Global, Permission::Load});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, true, true, false),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::LoadStoreCapability,
	                   Permission::LoadMutable});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, false, true, true, true),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::LoadStoreCapability,
	                   Permission::LoadMutable,
	                   Permission::LoadGlobal});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, true, true, false, false, false),
	  {Permission::Global, Permission::Load, Permission::Store});
	check_permissions(
	  MMIO_CAPABILITY(Uart, uart) /* check default permissions */,
	  {Permission::Global, Permission::Load, Permission::Store});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, true, true, false, false),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::Store,
	                   Permission::LoadStoreCapability});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(
	    Uart, uart, true, true, false, true, false),
	  {Permission::Global, Permission::Load, Permission::Store});
	check_permissions(MMIO_CAPABILITY_WITH_PERMISSIONS(
	                    Uart, uart, true, true, true, true, false),
	                  {Permission::Global,
	                   Permission::Load,
	                   Permission::Store,
	                   Permission::LoadStoreCapability,
	                   Permission::LoadMutable});
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

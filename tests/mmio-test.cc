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
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, false, false, false, false),
	  {Permission::Global});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, false, false, true, false),
	  {Permission::Global});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, false, false, false, true),
	  {Permission::Global});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, false, false, true, true),
	  {Permission::Global});
#endif
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, false, true, false, false),
	  {Permission::Global, Permission::Store});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, false, true, true, false),
	  {Permission::Global, Permission::Store, Permission::LoadStoreCapability});
#if !__has_attribute(cheriot_mmio)
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, false, true, false, true),
	  {Permission::Global, Permission::Store});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, false, true, true, true),
	  {Permission::Global, Permission::Store, Permission::LoadStoreCapability});
#endif
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, true, false, false, false),
	  {Permission::Global, Permission::Load});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, true, false, true, false),
	  {Permission::Global, Permission::Load, Permission::LoadStoreCapability});
#if !__has_attribute(cheriot_mmio)
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, true, false, false, true),
	  {Permission::Global, Permission::Load});
#endif
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, true, false, true, true),
	  {Permission::Global,
	   Permission::Load,
	   Permission::LoadStoreCapability,
	   Permission::LoadMutable});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, true, true, false, false),
	  {Permission::Global, Permission::Load, Permission::Store});
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, true, true, true, false),
	  {Permission::Global,
	   Permission::Load,
	   Permission::Store,
	   Permission::LoadStoreCapability});
#if !__has_attribute(cheriot_mmio)
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, true, true, false, true),
	  {Permission::Global, Permission::Load, Permission::Store});
#endif
	check_permissions(
	  MMIO_CAPABILITY_WITH_PERMISSIONS(Uart, uart, true, true, true, true),
	  {Permission::Global,
	   Permission::Load,
	   Permission::Store,
	   Permission::LoadStoreCapability,
	   Permission::LoadMutable});
	return 0;
}

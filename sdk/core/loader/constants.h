// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <assembly-helpers.h>

#ifdef __cplusplus
#	include <priv/riscv.h>

#	include "types.h"
#	include "../scheduler/loaderinfo.h"
namespace
{
	/// The return type of the loader, to pass information to the scheduler.
	struct SchedulerEntryInfo
	{
		/// The sentry to the switcher
		void *switcherPCC;
		/// The export entry for the scheduler entry
		void *schedulerExportEntry;
		/// Start of initialiser list.
		void *initialiserList;
		/// Thread descriptors for all threads
		ThreadLoaderInfo *threads;
	};
	using namespace loader;
	/*
	 * This structure will be allocated on the stack from assembly.  Make sure
	 * that its size is a multiple of 16 bytes to ensure that the stack is
	 * always 16-byte aligned.
	 */
	static_assert(sizeof(ThreadLoaderInfo) % 16 == 0);
	static_assert(sizeof(SchedulerEntryInfo) % 16 == 0);

	static constexpr size_t HazardPointersPerThread = 2;

} // namespace
#endif

EXPORT_ASSEMBLY_OFFSET(SchedulerEntryInfo, switcherPCC, 0);
EXPORT_ASSEMBLY_OFFSET(SchedulerEntryInfo, schedulerExportEntry, 8);
EXPORT_ASSEMBLY_OFFSET(SchedulerEntryInfo, initialiserList, 16);
EXPORT_ASSEMBLY_OFFSET(SchedulerEntryInfo, threads, 24);
EXPORT_ASSEMBLY_SIZE(SchedulerEntryInfo, 32);
EXPORT_ASSEMBLY_EXPRESSION(MIE_MEIE, priv::MIE_MEIE, 0x800);
EXPORT_ASSEMBLY_EXPRESSION(MIE_MTIE, priv::MIE_MTIE, 0x080);
EXPORT_ASSEMBLY_EXPRESSION(MSTATUS_MIE, priv::MSTATUS_MIE, 8);

EXPORT_ASSEMBLY_EXPRESSION(ThreadHeapHazards_size,
                           HazardPointersPerThread * sizeof(void *),
                           16);

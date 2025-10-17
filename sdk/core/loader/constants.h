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
		/// scheduler initial entry point for thread initialisation
		void *schedPCC;
		/// scheduler CGP for thread initialisation
		void *schedCGP;
		/// thread descriptors for all threads
		ThreadLoaderInfo threads[];
	};

	using namespace loader;

} // namespace
#endif

EXPORT_ASSEMBLY_OFFSET(SchedulerEntryInfo, threads, 16);
EXPORT_ASSEMBLY_EXPRESSION(MIE_MEIE, priv::MIE_MEIE, 0x800);
EXPORT_ASSEMBLY_EXPRESSION(MIE_MTIE, priv::MIE_MTIE, 0x080);
EXPORT_ASSEMBLY_EXPRESSION(MSTATUS_MIE, priv::MSTATUS_MIE, 8);
EXPORT_ASSEMBLY_SIZE(ThreadLoaderInfo, 16);

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#include <assembly-helpers.h>
#define IMAGE_HEADER_LOADER_CODE_START_OFFSET 0
#define IMAGE_HEADER_LOADER_CODE_SIZE_OFFSET 4
#define IMAGE_HEADER_LOADER_DATA_START_OFFSET 6
#define IMAGE_HEADER_LOADER_DATA_SIZE_OFFSET 10

#ifdef __cplusplus
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

	static_assert(CheckSize<offsetof(ImgHdr, loader.code.startAddress),
	                        IMAGE_HEADER_LOADER_CODE_START_OFFSET>::value,
	              "Offset of loader code start is incorrect");
	static_assert(CheckSize<offsetof(ImgHdr, loader.code.smallSize),
	                        IMAGE_HEADER_LOADER_CODE_SIZE_OFFSET>::value,
	              "Offset of loader code size is incorrect");
	static_assert(CheckSize<offsetof(ImgHdr, loader.data.startAddress),
	                        IMAGE_HEADER_LOADER_DATA_START_OFFSET>::value,
	              "Offset of loader data start is incorrect");
	static_assert(CheckSize<offsetof(ImgHdr, loader.data.smallSize),
	                        IMAGE_HEADER_LOADER_DATA_SIZE_OFFSET>::value,
	              "Offset of loader data size is incorrect");
} // namespace
#endif

EXPORT_ASSEMBLY_OFFSET(SchedulerEntryInfo, threads, 16);

// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#define IMAGE_HEADER_LOADER_CODE_START_OFFSET 0
#define IMAGE_HEADER_LOADER_CODE_SIZE_OFFSET 4
#define IMAGE_HEADER_LOADER_DATA_START_OFFSET 6
#define IMAGE_HEADER_LOADER_DATA_SIZE_OFFSET 10

#ifdef __cplusplus
#	include "types.h"
namespace
{
	/**
	 * Helper class for checking the size of a structure.  Used in static
	 * asserts so that the expected size shows up in compiler error messages.
	 */
	template<size_t Real, size_t Expected>
	struct CheckSize
	{
		static constexpr bool Value = Real == Expected;
	};

	using namespace loader;

	static_assert(CheckSize<offsetof(ImgHdr, loader.code.startAddress),
	                        IMAGE_HEADER_LOADER_CODE_START_OFFSET>::Value,
	              "Offset of loader code start is incorrect");
	static_assert(CheckSize<offsetof(ImgHdr, loader.code.smallSize),
	                        IMAGE_HEADER_LOADER_CODE_SIZE_OFFSET>::Value,
	              "Offset of loader code size is incorrect");
	static_assert(CheckSize<offsetof(ImgHdr, loader.data.startAddress),
	                        IMAGE_HEADER_LOADER_DATA_START_OFFSET>::Value,
	              "Offset of loader data start is incorrect");
	static_assert(CheckSize<offsetof(ImgHdr, loader.data.smallSize),
	                        IMAGE_HEADER_LOADER_DATA_SIZE_OFFSET>::Value,
	              "Offset of loader data size is incorrect");
	static_assert(CheckSize<offsetof(ImgHdr, switcher.entryPoint), 18>::Value,
	              "Offset of loader data size is incorrect");
} // namespace
#endif

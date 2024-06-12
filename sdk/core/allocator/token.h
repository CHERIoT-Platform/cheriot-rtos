// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

// This is the header used by the sealing manager internally.

#if !defined(__ASSEMBLER__)

#	include <cdefs.h>
#	include <cheri.hh>
#	include <stddef.h>
#	include <stdint.h>
#	include <token.h>
#	include <debug.hh>

/// Opaque type.  Sealing keys don't really point to anything.
struct SKeyStruct;

/**
 * The structure that represents a sealed object.
 *
 * The `data` field represents the unsealed object.  There is some subtle
 * complexity here: We must be able to precisely represent the unsealed object
 * but we must *also* be able to precisely represent the entire sealed object.
 *
 * For larger objects, the alignment of the top and bottom are stricter and so
 * we cannot simply place this header at the start of the allocation.  Instead,
 * we arrange the objects in the allocation as follows:
 *
 * ```
 * |Padding|Header|Unsealed Object|
 *         ^
 *         Address of sealed capability is here.
 * ```
 *
 * This means that there is a small amount of padding at the start of the
 * allocation, but the address of the capability points to the start of the
 * header.  For anything below 4072 bytes with the current encoding, this
 * amount of padding is zero and then we gradually increase it as the object
 * size increases.
 */
struct SObjStruct
{
	/// The sealing type for this object.
	uint32_t type;
	/// Padding for alignment
	uint32_t padding;
	/// The real data for this.
	char data[];
};

typedef struct SObjStruct TokenSObj;

constexpr size_t ObjHdrSize = offsetof(SObjStruct, data);

/// Helper for referring to a sealing key
struct SealingKey : public CHERI::Capability<SKeyStruct>
{
	/**
	 * Constructor.  Checks that this is a plausible sealing key (but does not
	 * check permissions).  If this is not a valid capability whose length is
	 * one and whose address and base match, then this will be set to null and
	 * all subsequent checks on it will fail.
	 */
	__always_inline SealingKey(SKeyStruct *rawPtr) : Capability(rawPtr)
	{
		// Sealing keys must be length-one, in-bounds capabilities.
		if (!is_valid() || (base() != address()) || (length() != 1))
		{
			ptr = nullptr;
		}
	}
};
/// Helper for referring to a sealed allocation.
using SealedAllocation = CHERI::Capability<SObjStruct>;

template<>
struct DebugFormatArgumentAdaptor<SealingKey>
{
	__always_inline static DebugFormatArgument construct(SealingKey value)
	{
		return {reinterpret_cast<uintptr_t>(
		          static_cast<const volatile void *>(value)),
		        DebugFormatArgumentKind::DebugFormatArgumentPointer};
	}
};

#endif

#include <assembly-helpers.h>

EXPORT_ASSEMBLY_OFFSET(TokenSObj, type, 0);
EXPORT_ASSEMBLY_OFFSET(TokenSObj, data, 8);

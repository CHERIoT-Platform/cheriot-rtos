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

/// Opaque type.  Sealing keys don't really point to anything.
struct SKeyStruct;

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

#endif

#include <assembly-helpers.h>

EXPORT_ASSEMBLY_OFFSET(TokenSObj, type, 0);
EXPORT_ASSEMBLY_OFFSET(TokenSObj, data, 8);

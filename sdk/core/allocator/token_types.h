// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

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
 *                ^
 *                Address of sealed capability is here.
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

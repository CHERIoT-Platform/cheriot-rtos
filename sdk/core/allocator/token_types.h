// Copyright Microsoft and CHERIoT Contributors.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

/**
 * Opaque type for token (un)sealing keys.
 *
 * Sealing keys don't really point to anything, but they are CHERI
 * capabilities, and so C/C++ sees them as pointer types.
 */
struct TokenKeyType;

/**
 * A sealed token object's header.
 *
 * Token objects require sub-object considerations.  We must be able to
 * precisely represent (in the sense of CHERI capability bounds) both
 *
 * - the token object payload (exposed when a token is unsealed) and
 *
 * - the entire sealed object (that is, static or dynamic allocation).
 *
 * For larger allocations, the alignment of the top and bottom are subject to
 * alignment requirements, and so we cannot simply place this header at the
 * start of the allocation.  Instead, we place the payload at the top of the
 * allocation and the header immediately before, leaving any requisite padding
 * at the base of the allocation.
 *
 * In the current CHERIoT capability encoding, all payloads smaller than 4072
 * bytes will have no such padding.  The padding gradually increases as the
 * token object payload increases in size.
 *
 * Sealed handles to token objects point at the start of the payload (that is,
 * immediately after the header) and have bounds that encompass the entire
 * underlying allocation, padding and all, while capabilities to the payload are
 * constructed the same address but bounds encompassing only the payload.
 *
 * This layout and choice of sealed pointer address guarantee that all pointers
 * involved use addresses that are internal to the underlying allocation and so
 * cannot be confused for pointers to the entire allocation (even if there is no
 * padding at the front thereof).
 *
 * All told, a token object's underlying allocation and its sealed handle(s)
 * may be depicted thus:
 *
 * ```
 * |Padding|Header|Object Payload|
 *                ^
 *                Address of sealed capability is here.
 * ```
 */
struct TokenObjectHeader
{
	/// The sealing type for this object.
	uint32_t type;
	/// Padding for alignment
	uint32_t padding;
};

/**
 * An opaque type used as the target type of capabilities pointing, as above,
 * to the boundary between the token object header and the associated payload.
 */
struct TokenObjectType;

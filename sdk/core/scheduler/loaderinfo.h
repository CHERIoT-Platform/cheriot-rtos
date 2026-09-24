#pragma once
#include <__cheri_sealed.h>
#include <cstddef>
#include <cstdint>

/**
 * Forward declaration of the "trusted stack" structure used by the switcher.
 * The scheduler sees these only as the target types of sealed capabilities, so
 * this incomplete type suffices.
 *
 * However, this file is shared between the scheduler and the loader.  The
 * latter has the actual, complete definitions of TrustedStackGeneric and
 * TrustedStack (from sdk/core/switcher/trusted-stack.hh) in scope, so we have
 * to match its use of a template type and a using declaration.
 */
template<size_t NFrames>
struct TrustedStackGeneric;

using TrustedStack = TrustedStackGeneric<0>;

/**
 * Info about a thread to be passed from loader to the scheduler. The
 * scheduler will take this record and initialise the thread block.
 */
struct ThreadLoaderInfo
{
	/// The trusted stack for this thread. This field should be sealed by
	/// the loader and contain populated PCC, CGP and CSP caps.
	CHERI_SEALED(TrustedStack *) trustedStack;
	/// Thread priority. The higher the more prioritised.
	uint16_t priority;
};

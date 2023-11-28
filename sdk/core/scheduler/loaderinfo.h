#pragma once
#include <cstddef>
#include <cstdint>

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
	TrustedStack *trustedStack;
	/// Thread priority. The higher the more prioritised.
	uint16_t priority;
};

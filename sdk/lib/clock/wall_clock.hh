#pragma once
/**
 * \file
 * The interface between the wall-clock time compartment and its supporting
 * library.
 */
#include <atomic>
#include <sys/time.h>
#include <time.h>

/**
 * A snapshot in the past of the wall-clock time for a monotonic-clock time.
 * This makes it possible to calculate the current time by computing the
 * displacement from the previous monotonic time and adding that to the current
 * wall-clock time.
 */
struct SynchronisedTime
{
	/// The value of the monotonic time when this was computed.
	clock_t monotonicTime;
	/// The value of the wall-clock time when this was computed.
	clock_t wallClockTime;
	/**
	 * The epoch for synchronisation.  This is incremented once when the epoch
	 * starts being incremented and again when it completes.  If the low bit is
	 * 1 when reading, an update is in progress.  If the start and end values
	 * differ, the read value is inconsistent and consumers should retry.
	 */
	std::atomic<uint32_t> updatingEpoch;
};

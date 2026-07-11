#include <debug.hh>
#include <limits>
#include <locks.hh>
#include <platform-time.h>
#include <sys/time.h>
#include <time.h>
#include <timeout.h>
#include <unwind.h>

// Appease clang-tidy and LSP by providing a stub implementation if we don't
// have a build configuration that provides this file yet.
#if __has_include("rtc_sources.hh")
#	include "rtc_sources.hh"
#else
#	include <tuple>
#	include <platform/concepts/wall_clock_source.hh>

struct NoWallClockSource
{
	static constexpr bool SupportsTimeSetting = false;
	static constexpr bool IsCheap             = true;
	int                   get_time(TimeoutArgument,
	                               clock_t &outRealTime,
	                               clock_t &outMonotonicTime,
	                               int     &outPriority);
};
static_assert(IsWallClockSource<NoWallClockSource>,
              "Placeholder is not a valid wall clock soure");

using ClockSources = std::tuple<NoWallClockSource>;
#endif

#include "wall_clock.hh"

namespace
{
	/// Debugging for this component
	using Debug = ConditionalDebug<false, "Wall clock">;

	/**
	 * Number of seconds to check expensive time sources.  Defaults to once per
	 * minute.
	 */
	constexpr uint64_t TimeToCheckExpensiveSources = 60ULL * CPU_TIMER_HZ;

	/**
	 * A tuple of clock sources.
	 */
	ClockSources clockSources;

	/**
	 * Helper to visit each clock source in `clockSources` and invoke
	 * `callback` on each.  The `callback` parameter should be a generic lambda
	 * (taking the argument as an `auto`-type parameter) and will be
	 * specialised to each concrete instance of the type.
	 */
	template<size_t I = 0>
	void visit_clock_sources(auto &&callback)
	{
		callback(std::get<I>(clockSources), I);
		if constexpr (I + 1 < std::tuple_size_v<ClockSources>)
		{
			visit_clock_sources<I + 1>(callback);
		}
	}

} // namespace

int clock_update_wall_clock(TimeoutArgument timeout)
{
	static FlagLock updatingLock;
	auto           &synchronisedTime = *SHARED_OBJECT_WITH_PERMISSIONS(
	  SynchronisedTime, wall_clock_time, true, true, false, false, false);
	if (LockGuard g{updatingLock, timeout})
	{
		// Error handler to ensure that the lock is released if a clock source
		// crashes.
		CHERIOT_DURING

		int     bestScore = std::numeric_limits<int>::min();
		clock_t bestWallClock;
		clock_t bestMonotonicClock;
		clock_t lastSynchronisationTime;

		// Get the current time.
		uint32_t epoch;
		do
		{
			epoch = synchronisedTime.updatingEpoch.load();
			// If the low bit is set then the time is being updated.  Wait for
			// the update to finish.
			if (epoch & 0x1)
			{
				// Wait for the update to finish
				synchronisedTime.updatingEpoch.wait(timeout, epoch);
				continue;
			}
			lastSynchronisationTime = synchronisedTime.monotonicTime;
		} while (epoch != synchronisedTime.updatingEpoch.load());

		// If the time since the last expensive update is above the threshold,
		// update from expensive sources as well as cheap ones.
		clock_t timeSinceLastSynchronisation =
		  platform_monotonic_time_read() - lastSynchronisationTime;
		bool shouldUpdateExpensive =
		  (lastSynchronisationTime == 0) ||
		  (timeSinceLastSynchronisation >= TimeToCheckExpensiveSources);

		bool anySucceeded = false;
		int  bestSource   = -1;

		// Check each source and find the one that gives the best score.
		visit_clock_sources([&](auto &&source, int i) {
			int     score;
			clock_t monotonicTime;
			clock_t wallclockTime;
			if (source.IsCheap || shouldUpdateExpensive)
			{
				CHERIOT_DURING
				if (source.get_time(
				      timeout, wallclockTime, monotonicTime, score) != 0)
				{
					return;
				}
				if (score > bestScore)
				{
					bestMonotonicClock = monotonicTime;
					bestWallClock      = wallclockTime;
					bestScore          = score;
					bestSource         = i;
					Debug::log(
					  "New best scores.  Wall clock: {}, monotonic: {}, "
					  "current time: {}",
					  bestWallClock,
					  bestMonotonicClock,
					  platform_monotonic_time_read());
				}
				CHERIOT_HANDLER
				Debug::log("Source {} trapped", i);
				CHERIOT_END_HANDLER
			}
			else
			{
				Debug::log("Skipping source {}", i);
			}
		});
		Debug::log("Best source was {}", bestSource);

		// If we found at least one usable source, update the others and
		// publish the time.
		if (bestScore != std::numeric_limits<int>::min())
		{
			visit_clock_sources([&](auto &&source, int i) {
				if constexpr (source.SupportsTimeSetting)
				{
					if (bestSource != i)
					{
						CHERIOT_DURING
						source.set_time(bestWallClock);
						CHERIOT_HANDLER
						CHERIOT_END_HANDLER
					}
				}
			});
			{
				Debug::log("Updating time.  New wall-clock time is {} at "
				           "monotonic time {}",
				           static_cast<int64_t>(bestWallClock),
				           static_cast<int64_t>(bestMonotonicClock));
				synchronisedTime.updatingEpoch++;
				synchronisedTime.monotonicTime = bestMonotonicClock;
				synchronisedTime.wallClockTime = bestWallClock;
				synchronisedTime.updatingEpoch++;
				synchronisedTime.updatingEpoch.notify_all();
			}
			return 0;
		}
		CHERIOT_HANDLER
		Debug::log("Clock compartment crashed updating");
		CHERIOT_END_HANDLER
		Debug::log("Returning failure");
	}

	return -ENOSYS;
}

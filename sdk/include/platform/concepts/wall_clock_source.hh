#pragma once

#include <concepts>
#include <time.h>
#include <timeout.h>

/**
 * Concept for wall-clock time sources (and sinks, if they support persisting
 * time).
 */
template<typename T>
concept IsWallClockSource =
  requires(T v, TimeoutArgument t, clock_t c, int i) {
	  /// Clock sources must expose whether they support setting the time.
	  { T::SupportsTimeSetting } -> std::same_as<const bool &>;
	  /**
	   * Clock sources must expose whether they are cheap enough to call
	   * every time a user asks for the clock to be updated or only after
	   * there is some reasonable possibility of clock drift.
	   */
	  { T::IsCheap } -> std::same_as<const bool &>;

	  /**
	   * Clock sources must provide a mechanism to get a matching pair of
	   * monotonic and wall-clock time and a priority.  This returns 0 on
	   * success.
	   */
	  { v.get_time(t, c, c, i) } -> std::same_as<int>;
  } &&
  ((T::SupportsTimeSetting == true) &&
     requires(T v) {
	     {
		     v.set_time(std::declval<TimeoutArgument>, std::declval<clock_t &>)
	     } -> std::same_as<int>;
     } ||
   requires { !T::SupportsTimeSetting; });

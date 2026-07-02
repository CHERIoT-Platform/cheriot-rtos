Design document for clocks
==========================

In CHERIoT RTOS today, we build everything in the core RTOS around a single clock source, which provides a monotonic clock.
We also have a real-time clock (implementing `gettimeofday`) in the network stack, exposed from the SNTP compartment.

POSIX defines a minimum of two clocks:

 - `CLOCK_MONOTONIC`, which corresponds to the current clock.
 - `CLOCK_REALTIME`, which is currently satisfied by the SNTP compartment but not integrated.

Some APIs that we want to be able to implement, such as the POSIX threading APIs, take timeouts relative to the realtime clock, which means we need that to not come from an optional component.

There is a single clock source for the monotonic clock, which is provided by the SoC and connected to an interrupt line so that the scheduler can wake processes based on it.
In contrast, there are often multiple RTCs:

 - A software-managed clock source from NTP or similar.
 - An on-SoC RTC device that persists on low-power modes.
 - An external RTC that has a separate battery backup and continues to increment time while the SoC is powered down.

These have different properties.
An off-chip RTC will always (after it's been set once) keep giving you time, but it might not be the correct time (drift of a second or more).
SNTP will give you a very accurate time, as long as you're talking to an NTP server with a sensible stratum.
Some clock sources are read-only (you can't set time from an NTP client) but some will want to persist time from higher-accuracy sources.
The subsystem that manages clocks needs to support multiple sources with different properties.

Exposing the realtime clock
---------------------------

The current mechanism that exposes the clock from the SNTP compartment to consumers is based on the Xen clock model and works well:

There is a shared memory object that contains a wall-clock time and the monotonic time at which it was correct, and an epoch.
Callers can read both of these atomically using a small loop and then calculate the current monotonic time as a displacement from the wall-clock time.
We may wish to make the epoch counter a futex: updates to the realtime clock are infrequent and it's useful to be able to watch for when the wall-clock time changes.

Scheduler always uses monotonic time
------------------------------------

The scheduler is designed to be simple and so should continue to always use the monotonic clock for timeouts.
APIs that want to wait for a realtime-clock-based update should convert the time to the monotonic time before calling scheduler APIs.
This conversion is intrinsically racy, but so is anything using realtime clocks for timeouts.

Implementations can work around this in one of two ways:

 - Sleep for a bounded amount of monotonic time (e.g. one second) and then redo the realtime to monotonic time calculation.
 - Use a multiwaiter and also wait on a futex that's signalled when the drift between monotonic and realtime clocks are 

Wall-clock compartment
---------------

The wall-clock compartment should own the clock sources.
Some clock sources are actually managed by other compartments, some are direct device access.
Devices should look somewhat like this:

```c++
struct ExampleClockSource
{
	/// This does support setting the time.
	static constexpr bool SupportsTimeSetting = true;

	/// Updating the time is lightweight and can be done frequently with low overhead.
	static constexpr bool IsCheap = true;

	/**
	 * Get the current time.  The return value indicates whether the time was updated.  Return values are:
	 *  - 0. Time was successfully updated.
	 *  - `-ECOMPARTMENTFAIL`, `-ENOTENOUGHTRUSTEDSTACK`, or
	 *   `-ENOTENOUGHSTACK`: Getting the time required a cross-compartment
	 *   call, which failed.
	 *  - `-ENOSYS`. The device (e.g. external RTC or network connection) is not currently available.
	 * 
	 * On successful return, this will set `outRealTime` to a wall-clock time
	 * and `outMonotonicTime` to a time at which it was valid.  These times
	 * must both be times that occurred between the start and end of the call.
	 * 
	 * The units for the real time are microseconds.  The units for the
	 * monotonic time are defined by the platform's monotonic clock.
	 * 
	 * The `outPriority` value will be set to a value specifying how confident
	 * this time source is that this is a good time.  Larger values indicate
	 * more confidence.
	 */
    int get_time(uint64_t &outRealTime, uint64_t &outMonotonicTime, int &outPriority);

	/**
	 * Set the current wall-clock time.  This return 0 on succcess, some
	 * negative error code on failure.  If `SupportsTimeSetting` is false, this
	 * should always return an error.
	 */
    int set_time(uint64_t realTime);
```

When asked to update the wall-clock time, the compartment should query each time source in turn, tracking the most confident response.
It should then set the time for any clock sources that reported a lower confidence and which support setting the time.
If two clocks report the same confidence, it should use the first.
Once the compartment has a new time, it should update the shared-memory object and signal its futex.

Updating the wall-clock time always requires an explicit call.
It is up to the firmware author to decide when and how often to do this (for example, once at start, once after the network comes up, once every hour).

Each time source should be a separate header that defines a type that implements the time source.
We need a mechanism in the board file to provide both the list of headers to include and the list of types to instantiate.

*Open question*: What should this look like?
Users can provide board files that patch an existing board file with JSON patch to add modify arrays quite easily, so if we list both the include files and patches as arrays, users can add and remove individual ones easily and xmake can synthesise a header that includes all of the types and provide their names.

Out of scope functionality
--------------------------

Time zones.
On a desktop operating system, the system should provide a rich set of localisation functionality because multiple applications will need this.
Embedded devices, in contrast, may not have a user interface at all (for example, they may talk to users by talking to an MQTT server that talks to a web app that handles localisation).
If they do need to understand time zones, they may need a restricted set of functionality (e.g. a user-provided time-zone offset from UTC) rather than a full time-zone database.
Anything related to time zones can be layered on top.



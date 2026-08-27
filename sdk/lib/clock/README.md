Clock support
=============

This directory contains two components, the `clock_helpers` library and the `wall_clock` compartment.
These work in tandem.
The `wall_clock` compartment manages clock sources and exposes a shared-memory object that allows mapping from the monotonic clock (which is expected to be readable from any compartment by reading an MMIO region or CSR) to a wall-clock time.
The `clock_helpers` library provides POSIX-compatible accessors for reading clocks.

The design for this is in [`docs/Clocks.md`](../../../docs/Clocks.md).

Adding a clock source
---------------------

To add a clock source, you must implement the clock-source concept and expose it to this compartment.
The concept is in the [platform/concepts/wall_clock_source.hh](../../include/platform/concepts/wall_clock_source.hh) header.
Implementations may be entirely inline in the header (for example, if the clock is a hardware device with a simple interface) or may call other compartments (for example, if the clock source is NTP or similar).

The `wall_clock` compartment will perform the entire update sequence with a lock held, so clock implementations do not need to handle reentrant invocation.
They should gracefully recover if they trap and not leave themselves in an unrecoverable state.

If your implementation is called `MyWallClockSource` then you may find it useful to add a line such as this to validate compliance with the concept:

```c++
static_assert(IsWallClockSource<MyWallClockSource>,
              "The SNTP wall-clock source must implement the required concept");
```

You will then need to add the following in the build logic for your target:

```lua
  after_load(function(target)
      import("core.project.project")
      local wall_clock = project.target("wall_clock")
      if (wall_clock) then
          print("Adding dependency on wall-clock compartment");
          target:add("deps", "wall_clock")
          wall_clock:add("includedirs", path.join(target:scriptdir(), "{relative path to where your header is in your target's directory}"))
          wall_clock:add("cheriot.clock_source_includes", "{name of your header}")
          wall_clock:add("cheriot.clock_source_types", "{name of the type for your clock source}")
      else
          raise("Did not find wall-clock compartment")
      end
  end)
```

This looks up the wall-clock compartment and *adds* values three property arrays:

 - `includedirs` defines the include directories where this compartment will look for headers.
 - `cheriot.clock_source_includes` defines the include files that the wall-clock compartment will include.
 - `cheriot.clock_source_types` defines the list of types that will be used for defining a read-time clock.

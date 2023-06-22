Timeouts in CHERIoT-RTOS
========================

CHERIoT RTOS aims to provide a consistent notion of timeout for APIs.

Ticks
-----

Timeouts are described in terms of 'ticks'.
A tick is the time between two scheduling events, bounded by a time specified in the [board description file](BoardDescriptions.md).
In the current scheduler design, the hardware timer fires once every tick and performs a context switch.
A future tickless design will likely adjust this slightly and redefine a tick an upper bound on a scheduler operation, such that a voluntary yield causes a tick to finish early.

The macros in [`tick_macros.h`](../sdk/include/tick_macros.h) provide helpers for converting between ticks and milliseconds.
These are approximate and code that has strong timing requirements should query a timer after waking from a timeout.

Timeouts
--------

Timeouts are described by the `Timeout` structure in [`timeout.h`](../sdk/include/timeout.h).
These describe the number of ticks that *have* elapsed and the number of ticks remaining that *may* elapse before a timeout occurs.
The special value `UnlimitedTimeout` indicates that an unbounded number of ticks may elapse.

Each function that may block should take a pointer to a `Timeout` structure.
This API structure is intended to allow chaining.
The same `Timeout*` is passed to any scheduler API that can suspend the calling thread and will time out if the remaining number reaches zero.
The caller may then inspect the `elapsed` field to see how many ticks have elapsed while blocking.

Note that the `elapsed` number of ticks at the end of a blocking operation may exceed the initial `remaining` value (i.e. the maximum timeout).
When a timeout expires, the thread becomes runnable but a higher-priority thread may still prevent it from running.

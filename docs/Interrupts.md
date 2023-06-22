Interrupt handling in CHERIoT RTOS
==================================

CHERIoT RTOS does not allow user code to run directly from the interrupt handler.
The scheduler has a separate stack that is used to run interrupt handler and this then maps interrupts to scheduler events.
This allows interrupts from asynchronous sources to be handled by threads of any priority.

Futex primer
------------

Interrupts are mapped to futexes and so it's first important to understand how futexes work.

A futex (fast userspace futex) is something of a misnomer on CHERIoT RTOS, where there is no kernel / userspace distinction (the scheduler is a component that is not trusted by the rest of the system for confidentiality or integrity).
The core idea for a futex is an atomic compare-and-wait operation.
A `futex_wait` call tests whether a 32-bit word contains the expected value and, if it does, suspends the calling thread until another thread calls `futex_wake` on the futex-word address.

This mechanism is intended to avoid missed wakeups.
Threads waking waiters are expected to modify the futex word and then call `futex_wake`.
This ensures that either the modification happens before the wait, in which case the comparison fails and the `futex_wait` call returns immediately, or after in which case it is fine to ignore this `futex_wake` because it is not related to the current value.

This primitive can be used to implement locks.
The [`locks.hh`](../sdk/include/locks.hh) file contains a flag lock and a ticket lock that use a futex, for example.

Futexes for interrupts
----------------------

Each interrupt number has a futex associated with it.
This futex contains a number that is incremented every time that the interrupt fires.
The scheduler then wakes any threads that are sleeping on that futex.
A thread that wants to block waiting for an interrupt reads the value of this futex word and then calls `futex_wait` to be notified when the word has been incremented one or more times.

This mechanism allows multiple threads to wait for the same interrupt and perform different bits of processing, for example a network stack may receive an interrupt to detect that a packet needs handling and a lower-priority thread may record telemetry on the number of packets that have been received.

The `interrupt_futex_get` requests the futex for a particular interrupt.
This returns a read-only capability that can be read directly to get the number of times that an interrupt has fired and can be used for `futex_wait`.

Note that, because interrupt futexes are just normal futexes, they can also be used with the [multiwaiter](../sdk/include/multiwater.h) API, for example to wait for an interrupt or a message from another thread, whichever happens first.

Acknowledging interrupts
------------------------

The scheduler will not acknowledge external interrupts until explicitly told to do so.
The `interrupt_complete` function marks the interrupt as having been handled.
An interrupt will not be delivered again until it has been acknowledged.

Interrupt capabilities
----------------------

The two calls related to interrupts both require an authorising capability.
These use the [static software-defined capability mechanism](SoftwareCapabilities.md).
A helper wrapper, `DECLARE_AND_DEFINE_INTERRUPT_CAPABILITY` is provided for defining these.
This takes an interrupt number as one of the arguments.
The build system populates the `InterruptName` enumeration with the interrupts that are defined in the board description file so these can be used as symbolic constants, rather than hard-coded numbers.

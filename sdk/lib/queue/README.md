Message queues
==============

Message queues, as described in [`queue.h`](../../include/queue.h).

This directory provides two targets.

 - The message queue library (`message_queue_library`) provides APIs for message queues that can be shared between two threads in the same compartment.
 - The message queue compartment (`message_queue`) wraps these in APIs that can be used from different compartments.

The library uses the `setjmp`-based error handler (see: [`unwind.h`](../../include/unwind.h)) to recover from invalid bounds or permissions.
If you are using the library and want to be robust in the presence of CHERI exceptions, you should either add `unwind_error_handler` as a dependency of your compartment or provide an error handler that calls `cleanup_unwind`.

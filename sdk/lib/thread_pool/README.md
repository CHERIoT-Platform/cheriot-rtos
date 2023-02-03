Simple thread pool
==================

This directory provides a simple thread pool that demonstrates the use of sealing and messages queues.
This provides an `async()` function that takes a lambda and will execute it in another thread.
Note that the lambda must not capture any variables with automatic storage or it will fault on execution.

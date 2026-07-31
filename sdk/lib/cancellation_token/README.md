Cancellation tokens
===================

Cancellation tokens are a primitive for building cooperative asynchronous termination.
APIs that may run for a long time and require asynchronous cancellation take a cancellation token as an argument.
Periodically, during their long operation, they check the cancellation state of the token and, if they have been instructed to do so, stop.
Cancellation tokens are associated with a *cancellation-token source*, which can *signal* the token to indicate that they should stop.
Each cancellation token supports arbitrary *fan out*: many asynchronous tasks can be signalled by a single cancellation token.

The cancellation-token APIs are documented in [`cancellation.h`](../../include/cancellation.h).

These are implemented in a compartment that handles the signalling end and a library that implements the fast paths.
Adding the `cancellation_token` compartment as dependency also adds the fast-path library.

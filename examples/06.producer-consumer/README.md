Producer-consumer
=================

This example shows how to run two threads in a producer-consumer relationship using a message queue provided by the scheduler.
In this simple example, the producer produces a sequence of integers and the consumer prints them, but the approach generalises.

Check the [`xmake.lua`](xmake.lua) file near the bottom to see how multiple threads are added.
Each one thread starts executing a specific entry point in a specific compartment and has some memory reserved for its stack and its trusted stack (used for cross-compartment calls).

This demo runs two threads, each of which spends most of its time in a single compartment.
The first challenge in setting up a producer-consumer system is bootstrapping: how do we get the handle for the message queue to both compartments?
The solution to this is to remember that threads and compartments are orthogonal.
In this example, the producer compartment sets up the message queue and then calls into the consumer to pass it the message queue handle.

During the `set_queue` call, both threads are in the consumer compartment.
The consumer thread is sleeping on a futex waiting for the queue to be initialised, the producer thread is executing the `set_queue` function and setting the queue in a global.
Global variables in a compartment are the simplest (and least flexible) way of communicating between threads.
Here the global is set in one thread an read in another.
Just before return, the `set_queue` call signals the futex, which causes the consumer thread to wake up.
This thread will then immediately block waiting for messages in the queue.

In this simple example, we set the timeout to allow blocking indefinitely, so the producer thread will run until either the queue is full or its scheduling quantum is exceeded.
At this point, the consumer thread will run until either it is interrupted or the queue is empty.

Sending integers from one thread to another is not particularly interesting but the same pattern can be used for any producer-consumer relationship between threads.

In general, the scheduler should be trusted for availability but not for confidentiality or integrity.
When you use the scheduler's message queue, it is able to see any data that you put into the queue.
You can use the techniques from the [sealing example](../5.sealing) to ensure that only the receiver can see the data, which means that you trust the scheduler only for availability (it can drop messages, but it can't tamper with them or leak them).
The thread pool (see the [thread pool tests](`../../tests/thread_pool-test.cc`)) uses sealing to ensure that neither the scheduler nor the thread-pool compartment can see the contents of lambdas that are dispatched to execute asynchronously.

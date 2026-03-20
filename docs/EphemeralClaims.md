Ephemeral Claims
================

Ephemeral claims are a fast-path claim mechanism in the CHERIoT RTOS.
While claims are persistent, they are also expensive, since their creation entails a cross compartment call to the allocator.
Ephemeral claims are temporary, but do not require a cross-compartment call and so avoid the associated overheads.
The primary use case of ephemeral claims is to hold an object alive early in a compartment's entry path, keeping it valid while the compartment processes it, possibly including taking a persistent claim of its own.

Ephemeral claims behave much like regular CHERIoT claims, with a few key differences:
- They belong to a thread rather than a compartment.
- They aren't bound to any quota object.
- Each thread may possess up to two ephemeral claims at any one time, and they must be on different objects.
- Ephemeral claim expire whenever the thread performs a compartment call or makes another ephemeral claim.

This introduces subtle race conditions. For example, the following sequence is racy:

```
Thread 1: ephemeral_claim(X)
Thread 1: claim(X)
 (!) Thread 1 is interrupted in the allocator before grabbing the lock
Thread 2: free(X)
```

In the above sequence, Thread 2 succeeds in its attempt to free `X`, since Thread 1 released its ephemeral claim on the object while making the cross-compartment call to the allocator to acquire a persistent claim.

Hazard Pointers
---------------

Under the hood, a thread makes an ephemeral claim by setting its hazard slots.
These are two `void *` hazard pointers stored in the thread's trusted stack, a per-thread data structure that resides in the switcher.
A thread can retrieve a write-only capability to its hazard slots through a call to the switcher API `switcher_thread_hazard_slots()`.

The allocator compartment has access to the hazard slots of all threads on the system, and when it goes to free an object, it first checks these slots.
If the object in question is pointed to by some hazard pointer, instead of being freed, it is put on the hazard quarantine.
Whenever the allocator performs an allocation or deallocation, it attempts to drain the hazard quarantine by rechecking each thread's hazard slots.

Allocator Epoch
---------------

The above scheme introduces a race condition:

```
Thread 1: free(X)
 (!) Thread 1 is interrupted in the allocator while going through the hazard list, after having read Thread 2's slots
Thread 2: sets hazard slot with X
Thread 1: complete the de-allocation of X
Thread 2: returns thinking it holds an ephemeral claim on X
```

Here, Thread 2 believes itself to hold an ephemeral claim on X, but the allocator did not observe the update to the hazard slots before deallocating the object.
To avoid such races, all threads must synchronize with the allocator when setting their hazard slots, using something like a sequence lock.
The allocator exposes a read-only epoch that it increments before and after it traverses the hazard list.
When the epoch is odd, the allocator is in the process of traversing the list, and so threads must refrain from setting their hazard slots.
This is handled in the `heap_claim_ephemeral()` API, which will wait on the epoch as long as it is odd.
Once the traversal is complete, the allocator increments the epoch again, resulting in an even value.

```
Thread 1: free(X), setting allocator epoch to odd
     (!) Thread 1 is interrupted in the allocator while going through the hazard list, after having read Thread 2's slots
Thread 2: checks allocator epoch, sees that it is odd, waits on it
Thread 1: finishes going through the hazard list, completes the de-allocation of X, and sets the epoch to even
Thread 2: checks allocator epoch, sees that it is even, sets the hazard slots, but then sees that its capabilities have been invalidated and concludes that the ephemeral claim failed and the object has been freed
```

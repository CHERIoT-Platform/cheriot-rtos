Ephemeral Claims
================

Ephemeral claims are a temporary claim mechanism which does not require a compartment-call into the allocator.
The primary use-case of ephemeral claims is to speed up code patterns that would require a full claim to do a few simple memory operations on an object.
For example, checking an object passed by an untrusted compartment at the entry-point of a compartment.

Ephemeral claims behave much like regular CHERIoT claims, with a few key differences:
- They belong to a *thread* executing in a specific compartment.
- They are not bound to a quota object.
- Each thread may possess up to two ephemeral claims at any one time, and they must be on different objects.
- Ephemeral claim expire whenever the thread performs a compartment call or makes another ephemeral claim.

Ephemeral claims can be performed with the following API (part of the compartment helpers library):

```
int heap_claim_ephemeral(TimeoutArgument  timeout,
                         const void      *ptr,
                         const void *ptr2 __if_cxx(= nullptr));
```

Next, we explain the internals of the ephemeral claims API.
We conclude with some common pitfalls.

Hazard pointers
---------------

Ephemeral claims are inspired by [hazard pointers](https://doi.org/10.1109/TPDS.2004.8).
Each thread holds a pointer to two *hazard slots* in its trusted stack.
Hazard slots are themselves `void *` pointers that may point to heap objects to be temporarily claimed.
Under the hood, `heap_claim_ephemeral` makes an ephemeral claim by retrieving a write-only capability to the thread's hazard slots through a call to the switcher API `switcher_thread_hazard_slots()`, and setting these slots.

The allocator compartment has access to the hazard slots of all threads on the system.
When it goes to free an object, it first checks these slots.
The hazard slots of all threads are stored contiguously, making them easy to iterate through by the allocator.
If the object to be freed is pointed to by some hazard pointer, it is put on the *hazard quarantine*.
Whenever the allocator performs an allocation or deallocation, it attempts to drain the hazard quarantine by rechecking each thread's hazard slots -- freeing any object which is not anymore referenced by hazard slots.

Synchronization
---------------

The `heap_claim_ephemeral` API synchronizes with the allocator when setting the hazard slots.
The allocator exposes a read-only epoch that it increments before and after it traverses the hazard list.
When the epoch is odd, the allocator is in the process of traversing the list, and so threads must refrain from setting their hazard slots.
Once the traversal is complete, the allocator increments the epoch again, resulting in an even value, at which point threads may again safely set their hazard slots.
This is to prevent a race-condition such as this one:

```
Thread 1: free(X)
 (!) Thread 1 is interrupted in the allocator while going through the hazard list, after having read Thread 2's slots
Thread 2: sets hazard slot with X
Thread 1: complete the de-allocation of X
Thread 2: returns thinking it holds an ephemeral claim on X
```

Here, Thread 2 believes itself to hold an ephemeral claim on X, but the allocator did not observe the update to the hazard slots before deallocating the object.
This is implemented correctly in `heap_claim_ephemeral`:

```
Thread 1: free(X), setting allocator epoch to odd
     (!) Thread 1 is interrupted in the allocator while going through the hazard list, after having read Thread 2's slots
Thread 2: checks allocator epoch, sees that it is odd, waits on it
Thread 1: finishes going through the hazard list, completes the de-allocation of X, and sets the epoch to even
Thread 2: checks allocator epoch, sees that it is even, sets the hazard slots, but then sees that its capabilities have been invalidated and concludes that the ephemeral claim failed and the object has been freed
```

Pitfalls
--------

We now expand on a few composition pitfalls when using ephemeral claims.

**Ephemeral claim before a full claim**

One should be careful composing ephemeral claims with full claims.
Ephemeral claims are dropped on a compartment call, and full claims require a compartment call.
This means that doing an ephemeral claim before a full claim is identical to not doing the ephemeral claim from a concurrency perspective.

For example:

```
Thread 1: ephemeral_claim(X)
Thread 1: claim(X)
 (!) Thread 1 is interrupted in the allocator before grabbing the lock
Thread 2: free(X)
```

In this sequence, Thread 2 succeeds in its attempt to free `X`, as Thread 1 released its ephemeral claim while making the cross-compartment call into the allocator for the full claim.
This is covered by a static analyzer warning.

**Ephemeral claim before acquiring a lock**

Another pitfall is doing an ephemeral claim before acquiring a lock.
This is something one might want to do to hold the lock for as little time as possible, as acquiring the ephemeral claim may yield.
If the lock is uncontended, this works fine.
If the lock is contended then the lock implementation will call `futex_timed_wait` to block, which will release the ephemeral claim.
This is unlikely to show up in testing, because unless another thread tries to concurrently free it the object, it looks correct.
This is also covered by a static analyzer warning.

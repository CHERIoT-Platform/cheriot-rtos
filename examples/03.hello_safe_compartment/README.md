A more secure compartmentalised hello world
===========================================

The [simple compartmentalised hello world example](../2.hello_compartment), we separated out control of the UART into an isolated compartment.
This prevented access to the UART device from other and ensured that output was not interleaved.

So, given that we've already compartmentalised it, what more security can we add?
Security always depends on the threat model.
In the previous example, the caller is always trusted to pass a valid null-terminated C string.
What happens if they don't?
The compartment will fail and will forcibly unwind to the caller.

In this particular example, it doesn't matter because the compartment doesn't hold any state and forcibly unwinding in the middle is fine, but remember that we said that disabling interrupts was not a good idea here, so let's replace this with a lock.
Without this, a long string on a blocking UART can prevent any other thread from making forward progress for a potentially unbounded amount of time.

The simplest locking primitive that the RTOS provides is a futex (slightly misnamed: futex is a contraction of fast userspace mutex, but the concept of userspace doesn't directly translate here).
This uses a 32-bit word to hold state and allows the scheduler to atomically compare the value against an expected value and sleep if they do not match, and correspondingly to wake up any threads that are sleeping on a particular address.
We are going to use futex to build a very simple spinlock.
It will check, with interrupts disabled (remember, not all targets support hardware atomics), whether the current value is 0 and call `futex_wait` to block if it isn't.
Once the value is 0, it will (still with interrupts disabled) set it to 1.
At the end, it will store a 1 and wake up all sleepers.
In the best case, this still requires one call into the scheduler, a real futex-based lock implementation should store whether there are waiters in the lock word and skip the wake if there are no waiters.

Now we have a problem.
If we fault while accessing the string that the caller provided then we will unwind without releasing the lock.
To avoid this, we need to check that the capability is readable and that there is a null byte somewhere before the end.

The `check_pointer` template is a helper for the first check.
This returns `true` if the pointer passed as the argument has the required permissions and is large enough for the type of the pointer.
The type is `char`, so this isn't particularly helpful, but we can then get the bounds of the capability and check that it does contain a null byte somewhere.
Once this is done, we don't trust the caller to provide a C string (containing a null byte somewhere) and so we use a C++ `std::string_view` with the length taken from the size of the capability.
This isn't quite right, because the pointer may be an array embedded in a larger structure, ideally you'd check for a null byte in the range.

Note that this example is advertised as *more* secure.
There is still one problem: A caller can pass a heap-allocated buffer and free it from another thread.
These are currently harder to fix.
The simplest solution is to do the checks with interrupts disabled and copy a chunk to a local buffer, then use that with interrupts enabled, and loop until you've processed the whole thing.
In the future, the allocator will allow pinning allocations to ensure that they cannot be freed while in use.

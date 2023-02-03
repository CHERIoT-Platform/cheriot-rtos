Handling errors in compartments
===============================

The [more secure compartmentalised hello world](../3.hello_safe_compartment) example demonstrated how to program defensively in compartments.
Sometimes, it's much simpler to just go ahead and assume that everything is fine and recover when it goes wrong.
The [UART compartment](uart.cc) in this example follows this philosophy.
The entry point is now very simple, with only two lines:

```c++
LockGuard g{lock};
Debug::log("Message provided by caller: {}", msg);
```

The first of these is an RAAI type that acquires a lock when it enters scope and releases it when it leaves.
This protects against concurrent entry and allows this compartment to serialise access to the UART.
The second line logs the message, with no concern about the capability being invalid.

So what happens when this goes wrong?
If the caller-provided capability is invalid for any reason (bounds too small, no load permission, not valid at all, freed in the middle of the call), the CPU will trap.
When this happens, the fault handler will see if the compartment has a function called `compartment_error_handler` and call it if so.
The error handler should have this signature:

```c++
extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval);
```

This is explained in more detail in [the error handling document](../../docs/ErrorHandling.md).
This compartment implements this, reports the fault to the UART for debugging, releases the lock, and unwinds to the calling compartment.
Note that, since this compartment is writing to the UART, this will result in interleaved output.
If you are using a board with a separate debugging UART then this can be avoided but the simulator has a single (output-only) UART.

In this function, we use the `extract_cheri_mtval` helper, which extracts the exception cause value and the register number responsible for the fault.
The first message that is logs should look something like this:

```
UART compartment: Detected BoundsViolation(1) trying to write to UART.  Register CS0(8) contained invalid value: 0x80000b50 (v:1 0x80000b4b-0x80000b50 l:0x5 o:0x0 p: - RWcgml -- ---)
```

This comes from the string that doesn't have a null terminator.
Note that the length (`l:`) is five bytes and the address is at the end of the range.
Any load of any width at that address via this capability will fault and the reported error (`BoundsViolation`) reflects this.

The next error comes from the call where the caller provided a write-only capability:

```
UART compartment: Detected PermitLoadViolation(18) trying to write to UART.  Register CS0(8) contained invalid value: 0x80000b4b (v:1 0x80000b4b-0x80000b50 l:0x5 o:0x0 p: - -W---- -- ---)
```

Note that the address here matches the start, the callee has not yet managed to read a single byte before faulting.
The fault here, `PermitLoadViolation` indicates that we tried to do something that required load (read) permission but did not have it.
The permissions at the end here show only `W` (write), so that's expected.
Compare this with the permissions in the previous fault, which did include `R` permission and so allowed the callee to use this capability right up until it fell off the end.

The Sail simulator can also provide information about exceptions if you pass `--trace=exception`.
Enabling this will give you a trace something like this:

```sh
$ /cheriot-tools/bin/cheriot_sim --trace=exception build/cheriot/cheriot/release/error-handling
Running file build/cheriot/cheriot/release/error-handling.
ELF Entry @ 0x80000000
tohost located at 0x80005bd8
UART compartment: Message provided by caller: helloCHERI BoundsViolation Reg=0b001000 PC=0x80004A04
...
```

This reports the address of the faulting instruction.
Try looking at the `.dump` file that the build produced (this may require building with `xmake -v`) in `build/cheriot/cheriot/release/error-handling.dump` and find the line corresponding to the fault.
In the dump corresponding to the run above, the line was:

```objdump
80004a04: 03 45 04 00  	clbu	a0, 0(cs0)
```

This is a capability load of a byte, which is the read from the C string provided by the caller.

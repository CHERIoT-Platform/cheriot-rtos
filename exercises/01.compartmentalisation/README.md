Compartmentalisation Exercise 1
===============================

This exercise starts from a firmware image that simulates a bug that gives an attacker arbitrary-code execution.
It reads JavaScript programs (as compiled bytecode) from the UART and executes them.
The JavaScript has a set of FFI functions exposed that allow arbitrary capability manipulation, including pointer chasing.
This simulates an attacker building a code-reuse attack, with a lot more power than is normally possible on a CHERI system.
The attacker can:

 - Read the stack capability, global capability, and program counter capability into any of eight (virtual) register slots.
 - Read the permissions, bounds, address, and length of a capability.
 - Read capabilities from memory via any of its eight capabilities.
 - Read 32-bit words from memory via any of its eight capabilities.
 - Set the address of a capability in its set.
 - Write capabilities from its set of eight via any of that set.
 - Run arbitrary JavaScript to perform any of the above actions, intermixed with other computation.

This is the equivalent of the one of the most powerful weird machines that it is possible to create on a CHERI system from code reuse attacks.

You can find documentation on the full set of the functions exposed for an attacker to use in [`cheri.js`](cheri.js).

The goal of this exercise is to apply compartmentalisation to limit the damage that an attacker with even this level of compromise can do.

Running the exercise
--------------------

There are two shell scripts that are useful for testing.
The first, `run_simulator.sh`, will compile and run the firmware in the CHERIoT Ibex simulator, with the UART connected to a named pipe.
This allows the second script, `load_js.sh`, to compile a JavaScript file and provide it directly to the simulator.

To test that everything is working, try the `hello.js` file.
If you run the two scripts in separate terminals, you should see something like this:

```
$ ./load_js.sh hello.js 
Output generated: /dev/null
428 bytes
```

And from the simulator:

```
$ ./run_simulator.sh 
...
JavaScript compartment: Secret stored at 0x2004cb8c (v:1 0x2004cb8c-0x2004cb90 l:0x4 o:0x0 p: G RWcgm- -- ---)
JavaScript compartment: Read 0x1ac bytes of bytecode
JavaScript compartment: 0xdf8 bytes of heap available
Hello world
```

Don't worry if the numbers don't match exactly.

The lines that start with magenta `JavaScript compartment' are debugging lines that are produced by `Debug::log` calls in C++.
Output from JavaScript does not have this prefix.

Compiling the JavaScript to bytecode requires Node.js and npm.
These can be installed from packages on most operating systems.
If you're in the dev container, use `sudo apt install npm`.

The attacker's challenge
------------------------

This system's memory also contains a secret.
This is set to a value based on the cycle timer when the first byte is read on the UART (on a real system, equivalent secrets would be provided from secure storage, a network service, or a cryptographically secure random number generator).
When the simulator starts, it will print a line something like this (after receiving some input - load the `hello.js` script to force it to appear):

```
JavaScript compartment: Secret stored at 0x2004cb8c (v:1 0x2004cb8c-0x2004cb90 l:0x4 o:0x0 p: G RWcgm- -- ---)
```

This gives you the location of the secret.
A real adversary may have leaked this by disassembling the firmware and finding where it's held in the memory map or by leaking the address via some other vulnerability.
CHERI doesn't allow an attacker to inject pointers, but this is within the globals region for the compromised compartment and so the [`leak.js`](leak.js) script is able to derive a capability to it from the value in the CGP register and then leak it.

This script has a line like this near the top:

```
const SecretAddress = 0x2004cb8c
```

If this doesn't match the value that the log message, change it so that it does.
**Note**: this location will change as you progress through the exercises, keep it up to date when testing.

When you run this script (with `./load_js.sh leak.js`), you will see something like this from the simulator:

```
JavaScript compartment: Read 0x278 bytes of bytecode
JavaScript compartment: 0xbf8 bytes of heap available
JavaScript compartment: Secret was 232333, you guessed 232333.
JavaScript compartment: CONGRATULATIONS! You correctly leaked the secret!
```

If you run it again, you will see that the secret value has changed, but you were still able to leak it.
We will address this failure mode in the first exercise.

Once this is fixed, the attacker may still be able to mount a denial of service attack.
Try loading the `crash.js` script.
This loads via an invalid capability and will therefore trigger a fault.

```
JavaScript compartment: Read 0x1cc bytes of bytecode
JavaScript compartment: 0xdf8 bytes of heap available
This should crash
swci_main exiting with return code 00
```

The simulator exits here because the last thread has exited.
We'll address this failure mode in the third exercise.


Exercise 1: Confidentiality
---------------------------

First, we want to protect the secret.
For the this exercise, we want to move the 

This will involve moving some code, since each compartment is one or more source files: individual source files end up in one compartment.
The current compartment is defined in the [`xmake.lua`](xmake.lua) file like this:

```
compartment("js")
    add_files("js.cc")
    add_files("secret.cc")
```


The names of the source files don't matter but the names of the compartments do.
You will notice that the entry point function is annotated like this:

```
void __cheri_compartment("js") run()
```

The `__cheri_compartment` here is similar to DLL import and export annotations: it specifies the compartment that this function belongs in.
If you try to compile it in a different compartment, you will get an error.
If a caller is in the same compartment, the compiler will insert a direct call, otherwise it will be a cross-compartment call.
The `__cheri_compartment` annotation should normally be placed on the prototype.
Thread entry points are a special case: they do not need a separate prototype because they are not normally called from other C/C++ source files.

This function is exported from the compartment because it's a thread entry point, described later in the `xmake.lua` like this:

```
target:values_set("threads", {
    {
        compartment = "js",
        priority = 1,
        entry_point = "run",
        stack_size = 0x800,
        trusted_stack_frames = 4
    }
})
```

The names of both the compartment ("js") and the entry-point function ("run") must match the C++ source, here.

When you add another compartment, you will also need to make sure that it is linked into the firmware image.
Look for this line:

```
    add_deps("js")
```

This is where the `js` compartment is added to the firmware image.

For the first exercise, you should move `secret.cc` into a different compartment.
This will require modifying `xmake.lua` to build it in a separate compartment and modifying the prototype in `secret.h` to indicate the compartment from which the functions are exposed.

Once you have completed this exercise, try modifying the `leak.js` script to see if you can make it work.
You shouldn't be able to.

Exercise 2: Fault isolation
---------------------------

Compartments, among their other benefits, provide fault isolation.
They limit the blast radius of an error to things within the compartment and things that explicitly communicate with that compartment.

For the next exercise, we want to move the JavaScript execution into a compartment so that, if it crashes, it doesn't take out the main run loop.
You can alternatively think of this as moving the code that does the I/O into a separate compartment, so that it's protected from failures in the code that runs the JavaScript.

When you split the code up, you will need to expose a function that runs the JavaScript bytecode (probably declared in a header, with the `__cheri_compartment` annotation) that the compartment that reads the bytecode can use to invoke the JavaScript interpreter.
You will also need to update the build system.

This will require slightly larger changes than the first exercise.
In the first exercise, our compartment boundary aligned with an existing software-engineering boundary.
The code handling the secret was already a conceptually separate component, we just made it a security boundary.

In this exercise, the code for the I/O and the code for the JavaScript VM are intermingled.
You will probably find it easier to do this exercise as three steps:

 1. Factor the code that handles the JavaScript VM (from the large block comment to the end of the loop iteration) into a separate function that just takes the bytecode buffer as an argument.
 2. Move that function into a separate file.
 3. Move that file into a separate compartment.

At the end of this refactoring, you should be able to run the `crash.js` script multiple times, without it actually crashing. 

Exercise 3: Resource cleanup
----------------------------

Just before the JavaScript VM starts, the simulator will report a line like this:

```
JavaScript compartment: 0xbf8 bytes of heap available
```

If you have correctly moved the JavaScript code to a new compartment, then you may notice that this number goes down every time you load the `crash.js` script.
This number is not the total amount of available heap memory, it is the amount that the compartment containing the JavaScript interpreter is authorised to allocate.
If this is exhausted, other compartments may still allocate memory from their quotas.
This means that our leak still has a constrained blast radius but it's still a problem.

Memory quotas are implemented via a capability model.
Each compartment may hold zero or more capabilities that authorise allocating memory, with different quotas.
By default, each one holds a capability accessed via the `MALLOC_CAPABILITY` macro that authorises it to allocate up to 4096 bytes.
This is configurable, see [`stdlib.h`](../../sdk/include/stdlib.h) for more information.

In `stdlib.h`, you may notice a function called `heap_free_all`.
This is a big hammer for resource cleanup: it frees *all* memory that was allocated with a specific capability.
You can use this to avoid memory leaks.

Most commonly, you will use this in concert with an *error handler*, as defined in [`compartment.h`](../../sdk/include/compartment.h):

```
enum ErrorRecoveryBehaviour
compartment_error_handler(struct ErrorState *frame,
                          size_t             mcause,
                          size_t             mtval);
```

If you implement this function in a compartment, it will be invoked when any synchronous fault (such as a CHERI exception) occurs.
This is a flexible mechanism that lets you rewrite the register file and resume, or just give up and return to the compartment that invoked this one.

Implement an error handler that does not attempt to recover from the error but instead frees all memory associated with the JavaScript compartment before unwinding to the caller.

Once this is done, rerunning the `crash.js` script should report the same amount of free memory each invocation.

This has demonstrated two uses of compartmentalisation:

 - The JavaScript interpreter is in a *sandbox*, the rest of the system is protected from bugs in it.
 - The secret is in a *safebox*, it is protected from bugs in the rest of the system.

The CHERIoT compartmentalisation model supports both of these abstractions and their composition: mutual distrust.

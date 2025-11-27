Smashing the stack: CHERIoT version
===============

The objective of this exercise is to leak a secret value from a victim
compartment by attacking a cross-compartment call that does not correctly
sanitise pointer inputs.

Although CHERIoT provides very strong protection against memory safety
vulnerabilities through unforgeable pointers and always on bounds checks,
attacks on the compartmentalisation model are still possible given a
sufficiently powerful vulnerability primitive. This is unlikely to be found in
real code but it's fun and instructive to see what it would look like.

The Scenario
------------

You (the attacker) have gained arbitrary code execution on the victim system via
a supply chain attack, but the security conscious victim has hidden their secret
number in a compartment using CHERIoT's support for compartmentalisation. To
leak the secret you'll have to find a way to exploit a weakness in the victim
compartment's exposed interfaces[^1]. Fortunately for you the victim left a
gaping security hole in the form of a fully attacker controller memory copy with
no validation of the arguments (`insecure_memcpy`)! Since you have access to the
whole stack before calling the victim it's possible to call the victim function
with arguments that point to the stack it is running with!

By modifying only [`attacker.cc`](attacker.cc) you should be able to leak the
secret contained in the global variable `secret` in [`victim.cc`](victim.cc).
This might seem trivial but if you're not familiar with CHERIoT you might
encounter some surprises and hopefully learn a thing or two along the way!

[^1]: or a weakness in the CHERIoT RTOS implementation of compartmentalisation.
If you think you've found one then let us know!

Getting started
---------------

To get set up with the CHERIoT toolchain quickly it's recommended to use the dev
container. Follow the [getting started guide](../../docs/GettingStarted.md) to
set up a development environment then compile and run the exercise code in the
CHERIoT Sail emulator:

```
cd exercises/02.stack_smash
xmake config --sdk=/cheriot-tools/
xmake run
```

You should see output similar to the following:

```
tohost located at 0x80006e48
attacker: Attacker compartment started
attacker: Attacker stack pointer: 0x80006d20 (v:1 0x80005e10-0x80006e10 l:0x1000 o:0x0 p: - RWcgml -- ---)
attacker: Adjusted stack pointer to victim frame: 0x80006ce0 (v:1 0x80005e10-0x80006e10 l:0x1000 o:0x0 p: - RWcgml -- ---)
attacker: Stack[0x80006ce0] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006ce8] = 0x800003a2 (v:1 0x80000250-0x800007d8 l:0x588 o:0x5 p: - R-cgm- Xa ---)
attacker: Stack[0x80006cf0] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006cf8] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006d00] = 0x80006d20 (v:1 0x80005e10-0x80006e10 l:0x1000 o:0x0 p: - RWcgml -- ---)
attacker: Stack[0x80006d08] = 0x80006ce0 (v:1 0x80005e10-0x80006e10 l:0x1000 o:0x0 p: - RWcgml -- ---)
attacker: Stack[0x80006d10] = 0x80007050 (v:1 0x80007050-0x80007050 l:0x0 o:0x0 p: G RWcgm- -- ---)
attacker: Stack[0x80006d18] = 0x8000543c (v:1 0x800053b8-0x80005688 l:0x2d0 o:0x5 p: G R-cgm- X- ---)
attacker: Stack[0x80006d20] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006d28] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006d30] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006d38] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006d40] = 0x80006d48 (v:1 0x80006d48-0x80006dc8 l:0x80 o:0x0 p: - RWcgml -- ---)
attacker: Stack[0x80006d48] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006d50] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
attacker: Stack[0x80006d58] = 0x0 (v:0 0x0-0x0 l:0x0 o:0x0 p: - ------ -- ---)
victim: Incorrect guess: 1
attacker: Failed to guess the secret.
attacker: Attacker compartment finished
SUCCESS
```

The example code shows how to dump a portion of the stack containing the
victim's stack at the time of the call.  In the above output you can see the
victim's return address spilled to the stack at `0x80006ce8`. All you have to do
is overwrite this with a suitable capability and you should be able to leak the
secret. Easy!

Tips
----

Unfortunately CHERIoT's support for debugging is currently a bit limited but you
can find some tips [here](../../docs/Debugging.md). You might find tracing
support helpful and note that after building you can find disassembly of the
firmware at `build/cheriot/cheriot/release/stack_smash.dump`.

To complete the example there's also `secure_memcpy` which is a fixed version of
the vulnerable interface with appropriate argument checks. If you can leak the
secret by calling this then let us know!

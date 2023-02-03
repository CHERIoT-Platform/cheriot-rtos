Debugging
=========

CHERIoT does not currently have support for interactive debugging.
For now, the debugging options are logging, assertions and tracing.

Debug logging
-------------

The SDK supports logging in C++ using [debug.hh](../sdk/include/debug.hh).
To use this you should include something like the following in your code:

```C++
#include <debug.hh>
constexpr bool DebugFoo = DEBUG_FOO;
using Debug = ConditionalDebug<DebugFoo, "Foo">;
```

Here `DEBUG_FOO` is a compile time macro that should be set to `true` or `false` to enable or disable debug output in the `Foo` module.
`"Foo"` is a prefix that will be prepended to debug messages.

The SDK's [xmake file](../sdk/xmake.lua) provides support for defining debug macros of the form `DEBUG_FOO`.
To use it call `debugOption("foo")` in your `xmake.lua` and add `add_rules("cherimcu.component-debug")` to your target definitions.
You can then specify `--debug-foo=true` when running `xmake config` to build with debugging enabled.

Once you have defined an alias for `ConditionalDebug` as above you can then use `Debug::log` to output relevant debug messages, for example:

```C++
uint32_t anInt = 42;
const char *aString = "CHERIoT";
Debug::log("The value of anInt is {}. aString is {}.", anInt, aString);
```

will output:

```
Foo: The value of anInt is 0x2a. aString is CHERIoT.
```

Note that `{}` in the template string is replaced by the formatted value of the variable passed in.
The formatter supports some common types such as integers and strings.
By default, it will attempt to use [magic_enum.hpp](../sdk/include/magic_enum/magic_enum.hpp) to pretty-print enumerations.
Pretty-printing enumerations can significantly increase firmware size and so can be disabled by defining `CHERIOT_AVOID_CAPRELOCS`, which will then fall back to printing their integer values.
Pointers are displayed as capabilities using the following format:

```
0x800058e0 (v:1 0x800058e0-0x800058ec l:0xc o:0xb p: G RWcgm- -- ---)
 address    tag    base       top    length otype     permissions
```

The capability permissions are displayed as a string of the form `G RWcgml Xa SU0`, where a letter indicates that a permission is present and dash in the corresponding position indicates it is absent.
The letters correspond to the permissions as follows:
- **G**lobal (GL)
- **R**eadable (LD)
- **W**riteable (SD)
- Load / store **c**apability (MC)
- Load **g**lobal (LG)
- Load **m**utable (LM)
- Store **l**ocal (SL)
- E**X**ecutable (EX)
- Execute with **a**ccess system registers (SR)
- **S**eal (SE)
- **U**nseal (US)
- User**0** (U0)

 Note that lower case letters denote permissions that are 'dependent' on other permissions.
 For example, load / store **c**apabilities requires either **R**ead or **W**rite; load **g**lobal requires both **R**ead and load / store **c**apabilities.

### Assertions / invariants

The `ConditionalDebug` class also supports assertions and invariants, for example:

```C++
Debug::Assert(anInt == 42, "anInt was not 42, was {}.", anInt);
```

If debugging is enabled this will test whether `anInt` is equal to 42.
If the condition is false a message will be printed with the line number of the assertion and the formatted message, for example:

```
answer.cc:13 Assertion failure in check_answer
anInt was not 42, was 0x36.
```

After printing the message the assertion failure will trigger a trap using a reserved instruction.
The effect of this trap will depend on the rest of the application, for example whether there is an [error handler](ErrorHandling.md) registered for the compartment.
Take care not to write conditions that have side-effects (e.g. using the `++` operator to increment a variable) because these will execute differently depending on whether debugging is enabled.

`Debug::Invariant` is identical to `Debug::Assert` except that the condition is checked even if debugging is not enabled.
It will cause a trap whenever the condition evaluates to false, but the message will be printed only if debugging is enabled.

Tracing
-------

For detailed debugging the Sail simulator supports instruction level tracing.
This can be enabled using the `-v` option.
By default it is extremely verbose, printing all memory accesses (including instruction fetch), instructions executed, and registers written, for example:

```
mem[X,0x80000000] -> 0x4081
[0] [M]: 0x80000000 (0x4081) c.li ra, 0
x1 <-  t:0 s:0 perms:0b0000000000000 type:0x00000000 offset:0x00000000 base:0x00000000 length:0x000000000

mem[X,0x80000002] -> 0x4101
[1] [M]: 0x80000002 (0x4101) c.li sp, 0
x2 <-  t:0 s:0 perms:0b0000000000000 type:0x00000000 offset:0x00000000 base:0x00000000 length:0x000000000
```

This slows down execution and makes it very difficult to see UART output, therefore tracing can be selectively enabled using `--trace=instr|reg|mem|exception|platform|all`.
For example `--trace=instr` will output just the instruction count, PC, opcode and disassembly for each instruction:

```
[0] [M]: 0x80000000 (0x4081) c.li ra, 0
[1] [M]: 0x80000002 (0x4101) c.li sp, 0
[2] [M]: 0x80000004 (0x4181) c.li gp, 0
[3] [M]: 0x80000006 (0x4501) c.li a0, 0
[4] [M]: 0x80000008 (0x2231) c.jal 268
[5] [M]: 0x80000114 (0x4201) c.li tp, 0
[6] [M]: 0x80000116 (0x4281) c.li t0, 0
```

It is possible to specify more than one `--trace` option to enable multiple kinds of trace output.

To see terminal output more easily when tracing you can redirect it to a file using the `-t / --terminal-log` option. For example, if using bash:

```
cheriot_sim --trace=instr --trace=reg -t terminal.txt <path to elf> >trace.txt
```

will run the given ELF file, putting the console output in `terminal.txt` and a trace with instructions and register writes in `trace.txt`.
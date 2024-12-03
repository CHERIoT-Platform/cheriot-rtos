CHERIoT RTOS
============

This repository contains the core RTOS for the [CHERIoT platform](https://aka.ms/cheriot-tech-report).
This is currently a *research project* that has been open sourced to enable wider collaboration.
It is not yet in a state where it should be used in production: in particular, security issues will currently be fixed in the main branch of the repo with no coordinated disclosure.

To use this, you will also need to install some dependencies.
The [getting started guide](docs/GettingStarted.md) describes in detail how to build these:

 - A [version of LLVM with CHERIoT support](https://github.com/CHERIoT-Platform/llvm-project/tree/cheriot)
 - An implementation of the ISA (e.g. [CHERIoT-Ibex](https://github.com/Microsoft/cheriot-ibex) or the emulator generated from [the formal model](https://github.com/Microsoft/cheriot-sail))

These dependencies are pre-installed in the dev container that will be automatically downloaded if you open this repository in Visual Studio Code or by hitting `.` to open it in GitHub Code Spaces.

To clone this repository, make sure that you use `git clone --recurse` so that you get submodules.
This repository contains symbolic links.
**IMPORTANT**: If you wish to clone this repository on *Windows*, make sure that you have enabled Developer Mode and run `git config --global core.symlinks true`.
You must do this *before* cloning the repository.

The [getting started guide](docs/GettingStarted.md) describes how to install these and how to build the test suite and examples in this repository.

The RTOS is privilege separated into a small number of core components as described in the [architecture document](docs/architecture.md).
The C/C++ extensions used by the compartmentalisation model are described in the [language extensions document](docs/LanguageExtensions.md).

If you have questions, please see the [frequently asked questions](docs/faq.md) document or raise an issue.


Building firmware images
------------------------

This repo contains the infrastructure for building CHERIoT firmware images.

**NOTE**: The build system is currently based on xmake, but we have encountered a number of issues with our use of xmake and may switch to an alternative build system at some point.

Clone this repo into your project and create an `xmake.lua` referring to it.
The file should start with this line:

```lua
includes("{path to this repo}/sdk")
```

This will enable debug and release configuration (specified with `-m {release,debug}`).
Both are compiled with `-Oz` (optimise for size, even at the expense of performance).

Next you need to specify that you want to use the compiler provided by this SDK:

```
set_toolchains("cheriot-clang")
```

Now you can add targets.
We provide helpers for creating library, compartment, and firmware targets.
These work just like normal xmake targets:

```
library("lib")
    add_files("shared_c_file.c", "shared_cxx_file.cc")

compartment("example")
    add_files("example/example.c")

firmware("example-firmware")
    add_deps("lib", "example")
    on_load(function(target)
        target:values_set("threads", {
            {
                compartment = "example",
                priority = 1,
                entry_point = "entry_point",
                stack_size = 0x400,
                trusted_stack_frames = 2
            }
        })
```

The firmware description specifies the compartments and libraries that this system depends on and specifies the threads.
Threads are listed as a Lua array of objects, each of which has the following keys:

 - `compartment` specifies the name of the compartment in which this thread starts.
 - `entry_point` specifies the name of the exported function from that compartment that the thread will start executing.
   This function must take no arguments and return `void`.
 - `stack_size` specifies the size, in bytes, of the stack for this thread.
 - `trusted_stack_frames` specifies the number of trusted stack frames (the maximum depth of cross-compartment calls possible on this thread).
   Note that any call that may yield is likely to require at least one additional trusted stack frame to call the scheduler so, for example, a blocking call to `malloc` requires three stack frames (the caller, the allocator, and the scheduler).

```sh
$ xmake config --sdk={path to CHERIoT LLVM tools}
$ xmake
```

This will create the output in `build/cheriot/cheriot/{release,debug}/{name of firmware target}`.
It will also create a `.dump` file in the same location giving the objdump output of the same target.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft
trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.

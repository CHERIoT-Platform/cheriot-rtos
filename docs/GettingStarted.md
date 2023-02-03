Getting started
===============

This repository contains a dev container that installs the toolchain and the gold model emulator.
The easiest way to use this is with GitHub Code Spaces: simply press . to open an editor with all of the tools set up.
You can achieve the same locally by cloning the repository and opening it in VS Code with the Dev Container Extension installed.

If you do not wish to use the dev container then please read the [section on how to build the dependencies](#building-the-dependencies).

The dev container installs the toolchain and emulator in `/cheriot-tools/bin`.
If you have installed them somewhere else then replace `/cheriot-tools/` with your install location in the following instructions.

Cloning the repository
----------------------

This repository contains submodules and so must be cloned with:

```sh
$ git clone --recurse https://github.com/microsoft/cheriot-rtos
```

Building the test suite
-----------------------

To make sure that everything is working, a good first step is to build the test suite.
If you are using the dev container with VS Code / GitHub Code Spaces, then this is the default target and so can be built by running the `XMake: Build` command from the command box.

Whether you are building the test suite in the dev container or elsewhere, you can build it from the command line as well.
The build system requires a configure step and a build step:

```sh
$ cd tests
$ xmake config --sdk=/cheriot-tools/
$ xmake -v
```

To get more verbose output, you can try adding `--debug-{loader,allocator,scheduler}=y` to the `xmake config` flags.
These will each turn on (very) verbose debugging output from the named components.

One of the final lines in the output should be something like:

```
[ 96%]: linking firmware build/cheriot/cheriot/release/test-suite
```

This tells you the path to the firmware image.
You can now try running it in the simulator:

```sh
$ /cheriot-tools/bin/cheriot_sim -V build/cheriot/cheriot/release/test-suite
```

The `-V` flag disables instruction-level tracing so that you can see the UART output more clearly.

Running the examples
--------------------

There are several examples in the [`examples/`](../examples/) directory of the repository.
These show how to use individual features of the platform.
Each of these is built and run in exactly the same way as the test suite.
For more detailed instructions [see the examples documentation](../examples/README.md).

Generating `compile_commands.json`
----------------------------------

The `clangd` language server protocol implementation depends on a `compile_commands.json` file to tell it the various flags for building.
If you are using the dev container then this is generated automatically but in other scenarios you must create it yourself from the test suite's build by running the following commands after you have configured the test suite:

```sh
$ cd tests
$ xmake project -k compile_commands ..
```

Building the dependencies
-------------------------

If you do not wish to use the dev container, you will need to build:

 - The LLVM-based toolchain with CHERIoT support
 - The emulator generated from the Sail formal model of the CHERIoT ISA
 - The xmake build tool

Building LLVM is fairly simple, but requires a fast machine and several GiBs of disk space.
Building the executable model requires a working ocaml installation.

### Building CHERIoT LLVM

To build LLVM, you will need `cmake` and `ninja` installed from your distribution's packaging system.
For example on Ubuntu Linux distributions you would need to run (as root):

```sh
# apt install ninja-build
# snap install cmake --classic
```

On FreeBSD:

```sh
# pkg ins cmake ninja
```

The version of LLVM that you need is in the `cheriot` branch of the [CHERI LLVM repository](https://github.com/CTSRD-CHERI/llvm-project/tree/cheriot).
This branch is temporary and will eventually be merged into the main CHERI LLVM branch and upstreamed to LLVM.

First, clone this repo somewhere convenient:

```sh
$ git clone --depth 1 https://github.com/CTSRD-CHERI/llvm-project cheriot-llvm
$ cd cheriot-llvm
$ git checkout cheriot
$ export LLVM_PATH=$(pwd)
```

If you want to do a custom build of LLVM, follow [their build instructions and adjust any configuration options that you want](https://www.llvm.org/docs/CMake.html).
If you want to just build something that works, keep reading.

Next create a directory for the build and enter it:

```sh
$ mkdir -p builds/cheriot-llvm
$ cd builds/llvm
```

This can be inside the source checkout but you may prefer to build somewhere else, for example on a different filesystem.
Next, use `cmake` to configure the build:

```sh
$ cmake ${LLVM_PATH} -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" -   DCMAKE_INSTALL_PREFIX=install -DLLVM_ENABLE_UNWIND_TABLES=NO -DLLVM_ENABLE_LLD=ON -DLLVM_TARGETS_TO_BUILD=RISCV -       DLLVM_DISTRIBUTION_COMPONENTS="clang;clangd;lld;llvm-objdump" -G Ninja
```

It is very strongly recommended that you do a release build, debug builds can take several minutes to compile files that take seconds with release builds.
The `LLVM_DISTRIBUTION_COMPONENTS` flag will let us build only the components that we want.
You can change the `install` location to somewhere else, for example `~/cheriot-tools` if you want a more memorable path.

Finally, build the toolchain:

```sh
$ export NINJA_STATUS='%p [%f:%s/%t] %o/s, %es'
$ ninja install-distribution
```

The first line here is optional, it will give you a more informative progress indicator.
At the end of this process, you will have the toolchain files installed in the location that you passed as the `CMAKE_INSTALL_PREFIX` option to `cmake` (the `install` directory inside your build directory if you didn't change the `cmake` line).
This will include:

 - `clang` / `clang++`, the C/C++ compiler.
 - `ld.lld`, the linker.
 - `llvm-objdump`, the tool for creating human-readable dumps of object code.
 - `clangd`, the language-server protocol implementation that is aware of our C/C++ extensions.

#### Configuring your editor

If your editor supports the language-server protocol then you should tell it to use the version of `clangd` that you have just built.
There are more ways of doing this than there are editors and so this is not an exhaustive set of instructions.
The `clangd` binary to use with any of them will be `bin/clangd` inside your LLVM install location.

If you are using VS Code, then you can install the [clangd extension](https://github.com/clangd/vscode-clangd).
Open its settings and find the entry labeled `Clangd: Path`.
This should be set to your newly built `clangd` location.

Vim and NeoVim have a number of language-server-protocol implementations.
The [ALE](https://github.com/dense-analysis/ale) extension has separate configuration options for C and C++.
You may wish to set these to the CHERIoT `clangd` only for paths that match a pattern where your CHERIoT code will live, by adding something like the following to your `.vimrc`:

```vim
autocmd BufNewFile,BufRead /home/myuser/cheriot/* let g:ale_c_clangd_executable = "/my/cheriot/llvm/install/bin/clangd"
autocmd BufNewFile,BufRead /home/myuser/cheriot/* let g:ale_cpp_clangd_executable = "/my/cheriot/llvm/install/bin/clangd"
```

### Building the emulator

The emulator is generated from a Sail formal model.
Sail is an ISA specification language that is implemented in ocaml.
None of the ocaml components (or any dependencies other than `libgmp.so`) are needed after the build and so you may prefer to run this build in a disposable container and just extract the emulator at the end.

The first step in building it is to install the dependencies, including your platform's version of `opam`, the ocaml package manager.
On Ubuntu:

```sh
# apt install opam z3 libgmp-dev cvc4
```

On FreeBSD:

```sh
# pkg ins ocaml-opam z3 gmp gmake pkgconf gcc
```

You can then install Sail with `opam`:

```sh
$ opam init --yes
$ opam install --yes sail
```

Now clone the CHERIoT Sail repository:

```sh
$ git clone --depth 1 --recurse https://github.com/microsoft/cheriot-sail
```

Make sure that all of the relevant opam environment variables are set and build the model:

```
$ cd cheriot-sail
$ eval $(opam env)
$ make csim
```

Note that this is a GNU Make build system, if you are running this on a non-GNU platform then GNU Make may be called `gmake` or similar and not `make`.

This will produce an executable in `c_emulator/cheriot_sim`.
This can be copied to another location such as somewhere in your path.

### Installing xmake

There are a lot of different ways of installing xmake and you should follow [the instructions that best match your platform](https://xmake.io/#/getting_started?id=installation).
For Ubuntu, you can do:

```sh
# add-apt-repository ppa:xmake-io/xmake
# apt update
# apt install xmake
```

